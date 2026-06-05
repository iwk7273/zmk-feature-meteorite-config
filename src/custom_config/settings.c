/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CUSTOM_CONFIG_SETTINGS_KEY "custom_config/state"

#if IS_ENABLED(CONFIG_SETTINGS)

#define CUSTOM_CONFIG_SCHEMA_V3 3U
#define CUSTOM_CONFIG_SCHEMA_V4 4U
#define CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT CUSTOM_CONFIG_SCHEMA_V4
#define CUSTOM_CONFIG_SCHEMA_VERSION_MARKER 0x80U
#define CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(version)                                                  \
    ((uint8_t)(CUSTOM_CONFIG_SCHEMA_VERSION_MARKER | (version)))

/* Frozen v3 on-flash payload (all scalars, no padding). DO NOT CHANGE: it must
 * match the historical layout so existing v3 data can still be read. */
struct __packed custom_config_v3_payload {
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
};

struct __packed custom_config_v3_stored {
    uint8_t schema_version;
    struct custom_config_v3_payload payload;
};

/* v4 on-flash payload. Packed and serialized field-by-field, decoupled from the
 * runtime struct layout so runtime padding never reaches flash. */
struct __packed custom_config_v4_ball_binding {
    uint16_t local_id;
    uint32_t param1;
    uint32_t param2;
};

struct __packed custom_config_v4_payload {
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
    uint8_t ball_sensitivity;
    uint8_t layer_profiles[ZMK_CUSTOM_CONFIG_MAX_LAYERS];
    struct custom_config_v4_ball_binding user1[ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS];
};

struct __packed custom_config_v4_stored {
    uint8_t schema_version;
    struct custom_config_v4_payload payload;
};

BUILD_ASSERT(sizeof(struct custom_config_v3_stored) == 11,
             "frozen v3 settings layout must not change");
BUILD_ASSERT(sizeof(struct custom_config_v4_stored) ==
                 1 + 11 + ZMK_CUSTOM_CONFIG_MAX_LAYERS + ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS * 10,
             "unexpected v4 settings size");

static bool settings_init;
static bool settings_need_resave;

static void custom_config_migrate_resave_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    int ret = zmk_custom_config_storage_save(zmk_custom_config_get());
    if (ret < 0) {
        LOG_WRN("Failed to persist migrated v4 custom config (%d)", ret);
    } else {
        LOG_INF("Persisted migrated v4 custom config");
    }
}

static K_WORK_DELAYABLE_DEFINE(custom_config_migrate_resave_work,
                               custom_config_migrate_resave_work_handler);

static void pack_v4(struct custom_config_v4_payload *dst, const struct zmk_custom_config *src) {
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
        dst->user1[d].local_id = src->user1[d].behavior_local_id;
        dst->user1[d].param1 = src->user1[d].param1;
        dst->user1[d].param2 = src->user1[d].param2;
    }
}

static void unpack_v4(struct zmk_custom_config *dst, const struct custom_config_v4_payload *src) {
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
        dst->user1[d].behavior_local_id = src->user1[d].local_id;
        dst->user1[d].param1 = src->user1[d].param1;
        dst->user1[d].param2 = src->user1[d].param2;
    }
}

/* Migrate a v3 payload into the runtime struct. Caller has filled cfg with
 * defaults first (so ball fields start at OFF / NORMAL / no-op). */
static void migrate_v3(struct zmk_custom_config *cfg,
                       const struct custom_config_v3_payload *v3) {
    cfg->cpi_idx = v3->cpi_idx;
    cfg->scroll_div = v3->scroll_div;
    cfg->rotation_idx = v3->rotation_idx;
    cfg->scroll_h_rev = v3->scroll_h_rev;
    cfg->scroll_v_rev = v3->scroll_v_rev;
    cfg->scaling_mode = v3->scaling_mode;
    cfg->scroll_scaling_mode = v3->scroll_scaling_mode;
    cfg->scroll_layer_1 = v3->scroll_layer_1;
    cfg->scroll_layer_2 = v3->scroll_layer_2;
    cfg->os_mode = v3->os_mode;

    /* Convert the old primary scroll layer into a SCROLL ball profile. The
     * legacy second scroll layer (scroll_layer_2) is retired, so it is not
     * migrated. */
    if (v3->scroll_layer_1 < ZMK_CUSTOM_CONFIG_MAX_LAYERS) {
        cfg->layer_profiles[v3->scroll_layer_1] = ZMK_BALL_PROFILE_SCROLL;
    }
}

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    struct custom_config_v4_stored stored = {
        .schema_version = CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(CUSTOM_CONFIG_SCHEMA_V4),
    };
    pack_v4(&stored.payload, cfg);

    int ret = settings_save_one(CUSTOM_CONFIG_SETTINGS_KEY, &stored, sizeof(stored));
    if (ret < 0) {
        LOG_WRN("Failed to save custom config (%d)", ret);
    } else {
        LOG_INF("Saved custom config schema=v%u (%zu bytes)", CUSTOM_CONFIG_SCHEMA_V4,
                sizeof(stored));
    }
    return ret;
}

int zmk_custom_config_storage_delete(void) {
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

    if (len == sizeof(struct custom_config_v4_stored)) {
        struct custom_config_v4_stored stored;
        int rc = read_cb(cb_arg, &stored, sizeof(stored));
        if (rc < 0) {
            return rc;
        }
        if ((size_t)rc != sizeof(stored) ||
            stored.schema_version != CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(CUSTOM_CONFIG_SCHEMA_V4)) {
            LOG_WRN("Ignoring custom config settings (v4 size, schema 0x%02x)",
                    stored.schema_version);
            return 0;
        }
        struct zmk_custom_config cfg;
        zmk_custom_config_set_defaults(&cfg);
        unpack_v4(&cfg, &stored.payload);
        zmk_custom_config_handle_loaded_settings(&cfg);
        settings_init = true;
        return 0;
    }

    if (len == sizeof(struct custom_config_v3_stored)) {
        struct custom_config_v3_stored stored;
        int rc = read_cb(cb_arg, &stored, sizeof(stored));
        if (rc < 0) {
            return rc;
        }
        if ((size_t)rc != sizeof(stored) ||
            stored.schema_version != CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(CUSTOM_CONFIG_SCHEMA_V3)) {
            LOG_WRN("Ignoring custom config settings (v3 size, schema 0x%02x)",
                    stored.schema_version);
            return 0;
        }
        struct zmk_custom_config cfg;
        zmk_custom_config_set_defaults(&cfg);
        migrate_v3(&cfg, &stored.payload);
        zmk_custom_config_handle_loaded_settings(&cfg);
        settings_init = true;
        settings_need_resave = true; /* persist as v4 after boot settles */
        LOG_INF("Migrated custom config settings v3 -> v4");
        return 0;
    }

    LOG_WRN("Ignoring custom config settings with incompatible size %zu", len);
    return 0;
}

static int custom_feature_settings_commit(void) {
    zmk_custom_config_commit_settings(settings_init);
    if (settings_need_resave) {
        settings_need_resave = false;
        /* Re-save in v4 form off the settings load path to avoid writing while
         * the settings backend is mid-load. saved == current after commit, so
         * this only rewrites the on-flash representation (dirty stays false). */
        k_work_reschedule(&custom_config_migrate_resave_work,
                          K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
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
