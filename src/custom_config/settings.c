/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CUSTOM_CONFIG_SETTINGS_KEY "custom_config/state"

#if IS_ENABLED(CONFIG_SETTINGS)

/*
 * APPEND-ONLY on-flash layout.
 *
 * The persisted payload (struct custom_config_payload) evolves by APPENDING
 * fields only. Existing fields must NEVER be reordered, resized, removed, or
 * repurposed. With that discipline the reader needs no per-version migration
 * code: a stored payload is always a prefix of the current layout, so
 *   - shorter (older) data is read as far as it goes and the remaining (newer)
 *     fields fall back to their defaults, and
 *   - longer (newer-than-us) data is read up to the part we understand and the
 *     trailing bytes are ignored.
 * See custom_feature_settings_set().
 *
 * The schema version is therefore NOT bumped for additive changes. It only
 * changes for a genuinely breaking change (a field whose meaning, size, or
 * order must change). Bumping it makes firmware that predates the break reject
 * this firmware's data (a one-time reset-to-defaults on that upgrade), which is
 * the intended fallback for the rare breaking case.
 */
#define CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT 4U
#define CUSTOM_CONFIG_SCHEMA_VERSION_MARKER 0x80U
#define CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(version)                                                  \
    ((uint8_t)(CUSTOM_CONFIG_SCHEMA_VERSION_MARKER | (version)))

struct __packed custom_config_ball_binding {
    uint16_t local_id;
    uint32_t param1;
    uint32_t param2;
};

/* On-flash payload. Packed and serialized field-by-field, decoupled from the
 * runtime struct layout so runtime padding never reaches flash. APPEND new
 * fields at the END only (see the layout note above). */
struct __packed custom_config_payload {
    /* Frozen schema-v3 prefix (order + size must never change). */
    uint8_t cpi_idx;
    uint8_t scroll_div;
    uint8_t rotation_idx;
    uint8_t scroll_h_rev;
    uint8_t scroll_v_rev;
    uint8_t scaling_mode;
    uint8_t scroll_scaling_mode;
    uint8_t scroll_layer_1;
    uint8_t scroll_layer_2;
    uint8_t os_mode;
    /* Appended for ball profiles (was "schema v4"). */
    uint8_t ball_sensitivity;
    uint8_t layer_profiles[ZMK_CUSTOM_CONFIG_MAX_LAYERS];
    struct custom_config_ball_binding user1[ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS];
    /* Keyboard behavior and power-management settings. */
    uint16_t mod_tap_tapping_term_ms;
    uint16_t layer_tap_tapping_term_ms;
    uint16_t idle_timeout_s;
    uint16_t idle_sleep_timeout_s;
    uint8_t mod_tap_flavor;
    uint16_t mod_tap_quick_tap_ms;
    uint16_t mod_tap_require_prior_idle_ms;
    uint8_t layer_tap_flavor;
    uint16_t layer_tap_quick_tap_ms;
    uint16_t layer_tap_require_prior_idle_ms;
    /* Append future fields HERE ONLY. */
};

struct __packed custom_config_stored {
    uint8_t schema_version;
    struct custom_config_payload payload;
};

/* Pin the frozen v3 prefix so a layout edit that would break reading old data
 * fails the build instead of silently corrupting it. These offsets must never
 * change; new fields only ever extend the payload past them. */
BUILD_ASSERT(offsetof(struct custom_config_payload, os_mode) == 9,
             "frozen v3 prefix of custom_config_payload must not change");
BUILD_ASSERT(offsetof(struct custom_config_payload, ball_sensitivity) == 10,
             "appended fields must follow the frozen v3 prefix");
BUILD_ASSERT(offsetof(struct custom_config_payload, layer_profiles) == 11 &&
                 offsetof(struct custom_config_payload, user1) == 27,
             "already-shipped appended fields must keep their on-flash offsets");
BUILD_ASSERT(offsetof(struct custom_config_payload, mod_tap_flavor) == 75,
             "hold-tap settings must remain appended after the shipped 75-byte payload");
/* Pin the total on-flash size too: it may only ever GROW, by appending fields at
 * the end of custom_config_payload (update the expected value here when doing
 * so). Any other size change means an existing field was resized or removed. */
BUILD_ASSERT(sizeof(struct custom_config_stored) == 86,
             "custom_config_stored size changed: only appending at the end of "
             "custom_config_payload is allowed (then update this assert)");

static bool settings_init;
static bool settings_need_resave;

static void custom_config_resave_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    /* Persist the SAVED snapshot, not the live config. A Studio edit can change
     * the live (current) config without saving during the debounce window before
     * this work runs; resaving current would silently persist that unsaved draft
     * and leave flash inconsistent with a later discard. Convergence only needs
     * the saved baseline rewritten in the full layout. */
    int ret = zmk_custom_config_storage_save(zmk_custom_config_saved_get());
    if (ret < 0) {
        LOG_WRN("Failed to persist custom config in current layout (%d)", ret);
    } else {
        LOG_INF("Persisted custom config in current layout");
    }
}

static K_WORK_DELAYABLE_DEFINE(custom_config_resave_work, custom_config_resave_work_handler);

/* The runtime and on-flash binding structs differ only in the local-id field
 * name (behavior_local_id vs local_id); these mirror helpers keep that mapping
 * in one place so pack and unpack cannot drift apart. */
static void pack_ball_binding(struct custom_config_ball_binding *dst,
                              const struct zmk_custom_config_ball_binding *src) {
    dst->local_id = src->behavior_local_id;
    dst->param1 = src->param1;
    dst->param2 = src->param2;
}

static void unpack_ball_binding(struct zmk_custom_config_ball_binding *dst,
                                const struct custom_config_ball_binding *src) {
    dst->behavior_local_id = src->local_id;
    dst->param1 = src->param1;
    dst->param2 = src->param2;
}

static void pack_payload(struct custom_config_payload *dst, const struct zmk_custom_config *src) {
    dst->cpi_idx = src->cpi_idx;
    dst->scroll_div = src->scroll_div;
    dst->rotation_idx = src->rotation_idx;
    dst->scroll_h_rev = src->scroll_h_rev;
    dst->scroll_v_rev = src->scroll_v_rev;
    dst->scaling_mode = src->scaling_mode;
    dst->scroll_scaling_mode = src->scroll_scaling_mode;
    dst->scroll_layer_1 = src->scroll_layer_1;
    dst->scroll_layer_2 = src->scroll_layer_2;
    dst->os_mode = src->os_mode;
    dst->ball_sensitivity = src->ball_sensitivity;
    for (int i = 0; i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
        dst->layer_profiles[i] = src->layer_profiles[i];
    }
    for (int d = 0; d < ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS; d++) {
        pack_ball_binding(&dst->user1[d], &src->user1[d]);
    }
    dst->mod_tap_tapping_term_ms = src->mod_tap_tapping_term_ms;
    dst->layer_tap_tapping_term_ms = src->layer_tap_tapping_term_ms;
    dst->idle_timeout_s = src->idle_timeout_s;
    dst->idle_sleep_timeout_s = src->idle_sleep_timeout_s;
    dst->mod_tap_flavor = src->mod_tap_flavor;
    dst->mod_tap_quick_tap_ms = src->mod_tap_quick_tap_ms;
    dst->mod_tap_require_prior_idle_ms = src->mod_tap_require_prior_idle_ms;
    dst->layer_tap_flavor = src->layer_tap_flavor;
    dst->layer_tap_quick_tap_ms = src->layer_tap_quick_tap_ms;
    dst->layer_tap_require_prior_idle_ms = src->layer_tap_require_prior_idle_ms;
}

static void unpack_payload(struct zmk_custom_config *dst, const struct custom_config_payload *src) {
    dst->cpi_idx = src->cpi_idx;
    dst->scroll_div = src->scroll_div;
    dst->rotation_idx = src->rotation_idx;
    dst->scroll_h_rev = src->scroll_h_rev;
    dst->scroll_v_rev = src->scroll_v_rev;
    dst->scaling_mode = src->scaling_mode;
    dst->scroll_scaling_mode = src->scroll_scaling_mode;
    dst->scroll_layer_1 = src->scroll_layer_1;
    dst->scroll_layer_2 = src->scroll_layer_2;
    dst->os_mode = src->os_mode;
    dst->ball_sensitivity = src->ball_sensitivity;
    for (int i = 0; i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
        dst->layer_profiles[i] = src->layer_profiles[i];
    }
    for (int d = 0; d < ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS; d++) {
        unpack_ball_binding(&dst->user1[d], &src->user1[d]);
    }
    dst->mod_tap_tapping_term_ms = src->mod_tap_tapping_term_ms;
    dst->layer_tap_tapping_term_ms = src->layer_tap_tapping_term_ms;
    dst->idle_timeout_s = src->idle_timeout_s;
    dst->idle_sleep_timeout_s = src->idle_sleep_timeout_s;
    dst->mod_tap_flavor = src->mod_tap_flavor;
    dst->mod_tap_quick_tap_ms = src->mod_tap_quick_tap_ms;
    dst->mod_tap_require_prior_idle_ms = src->mod_tap_require_prior_idle_ms;
    dst->layer_tap_flavor = src->layer_tap_flavor;
    dst->layer_tap_quick_tap_ms = src->layer_tap_quick_tap_ms;
    dst->layer_tap_require_prior_idle_ms = src->layer_tap_require_prior_idle_ms;
}

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    struct custom_config_stored stored = {
        .schema_version = CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT),
    };
    pack_payload(&stored.payload, cfg);

    int ret = settings_save_one(CUSTOM_CONFIG_SETTINGS_KEY, &stored, sizeof(stored));
    if (ret < 0) {
        LOG_WRN("Failed to save custom config (%d)", ret);
    } else {
        LOG_INF("Saved custom config schema=v%u (%zu bytes)", CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT,
                sizeof(stored));
    }
    return ret;
}

int zmk_custom_config_storage_delete(void) {
    /* Drop any deferred resave scheduled after loading an old/short payload:
     * left pending, it would fire after this delete and re-create the key with
     * a frozen copy of today's defaults, so a later firmware/DT with different
     * defaults would never take effect. */
    k_work_cancel_delayable(&custom_config_resave_work);
    int ret = settings_delete(CUSTOM_CONFIG_SETTINGS_KEY);
    if (ret < 0) {
        LOG_WRN("Failed to delete custom config settings (%d)", ret);
    } else {
        LOG_INF("Deleted custom config settings");
    }
    return ret;
}

static int custom_feature_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                       void *cb_arg) {
    if (!settings_name_steq(name, "state", NULL)) {
        return -ENOENT;
    }

    if (len < 1) {
        LOG_WRN("Ignoring empty custom config settings");
        return 0;
    }

    /* Start from defaults so any field absent from a shorter (older) stored
     * payload keeps its default value. Pre-pack the defaults into the on-flash
     * layout, then overlay the stored prefix on top: read_cb fills the first
     * `to_read` bytes (schema byte + the payload prefix that was actually
     * stored) and the rest stays at the packed defaults. */
    struct zmk_custom_config cfg;
    zmk_custom_config_set_defaults(&cfg);

    struct custom_config_stored stored;
    pack_payload(&stored.payload, &cfg);

    size_t to_read = MIN(len, sizeof(stored));
    int rc = read_cb(cb_arg, &stored, to_read);
    if (rc < 0) {
        return rc;
    }
    if ((size_t)rc != to_read) {
        LOG_WRN("Short read of custom config settings (%d/%zu)", rc, to_read);
        return 0;
    }

    if ((stored.schema_version & CUSTOM_CONFIG_SCHEMA_VERSION_MARKER) == 0) {
        LOG_WRN("Ignoring custom config settings (bad marker 0x%02x)", stored.schema_version);
        return 0;
    }

    uint8_t version = stored.schema_version & ~CUSTOM_CONFIG_SCHEMA_VERSION_MARKER;
    if (version > CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT) {
        /* Written by a newer firmware whose layout may have broken compatibility
         * (a schema bump only happens for breaking changes). Discard rather than
         * misread it; defaults stay in effect. */
        LOG_WRN("Ignoring custom config settings from newer schema v%u (current v%u)", version,
                CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT);
        return 0;
    }

    unpack_payload(&cfg, &stored.payload);
    zmk_custom_config_handle_loaded_settings(&cfg);
    settings_init = true;

    if (len < sizeof(struct custom_config_stored)) {
        /* Flash holds an older/shorter prefix. Correctness does not depend on
         * rewriting it (the prefix read fills defaults every boot), but resaving
         * the full current layout converges flash and keeps dirty=false after a
         * clean load. Deferred off the settings load path (see commit). */
        settings_need_resave = true;
        LOG_INF("Custom config stored as %zu B prefix; will resave full %zu B layout", len,
                sizeof(struct custom_config_stored));
    }
    return 0;
}

static int custom_feature_settings_commit(void) {
    zmk_custom_config_commit_settings(settings_init);
    if (settings_need_resave) {
        settings_need_resave = false;
        /* Re-save in the full current layout off the settings load path to avoid
         * writing while the settings backend is mid-load. saved == current after
         * commit, so this only rewrites the on-flash representation (dirty stays
         * false). */
        k_work_reschedule(&custom_config_resave_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_feature, "custom_config", NULL,
                               custom_feature_settings_set, custom_feature_settings_commit, NULL);
#else
int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    ARG_UNUSED(cfg);
    return 0;
}

int zmk_custom_config_storage_delete(void) { return 0; }
#endif
