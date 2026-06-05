/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct meteorite_config_state {
    struct zmk_custom_config current;
    struct zmk_custom_config saved;
    struct zmk_custom_config defaults;
    bool dirty;
};

static struct meteorite_config_state custom_config_state;
#define custom_config (custom_config_state.current)

__weak void zmk_custom_config_changed(const struct zmk_custom_config *cfg) { ARG_UNUSED(cfg); }

static int zmk_custom_config_save_os_mode_now(uint8_t os_mode);

#if IS_ENABLED(CONFIG_SETTINGS)
static uint8_t custom_config_pending_os_mode;

static void custom_config_os_mode_save_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    uint8_t os_mode = custom_config_pending_os_mode;
    if (custom_config.os_mode != os_mode) {
        LOG_DBG("Skipping stale custom config OS mode save");
        return;
    }

    int ret = zmk_custom_config_save_os_mode_now(os_mode);
    if (ret < 0) {
        LOG_WRN("Failed to save custom config OS mode (%d)", ret);
    }
}

static K_WORK_DELAYABLE_DEFINE(custom_config_os_mode_save_work,
                               custom_config_os_mode_save_work_handler);
#endif

static bool ball_binding_equals(const struct zmk_custom_config_ball_binding *a,
                                const struct zmk_custom_config_ball_binding *b) {
    return a->behavior_local_id == b->behavior_local_id && a->param1 == b->param1 &&
           a->param2 == b->param2;
}

/* Explicit field comparison (not memcmp) so struct padding introduced by the
 * uint16/uint32 ball binding members can never produce a false-positive dirty. */
static bool zmk_custom_config_equals(const struct zmk_custom_config *a,
                                     const struct zmk_custom_config *b) {
    if (a->cpi_idx != b->cpi_idx || a->scroll_div != b->scroll_div ||
        a->rotation_idx != b->rotation_idx || a->scroll_h_rev != b->scroll_h_rev ||
        a->scroll_v_rev != b->scroll_v_rev || a->scaling_mode != b->scaling_mode ||
        a->scroll_scaling_mode != b->scroll_scaling_mode ||
        a->scroll_layer_1 != b->scroll_layer_1 || a->scroll_layer_2 != b->scroll_layer_2 ||
        a->os_mode != b->os_mode || a->ball_sensitivity != b->ball_sensitivity) {
        return false;
    }
    for (int i = 0; i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
        if (a->layer_profiles[i] != b->layer_profiles[i]) {
            return false;
        }
    }
    for (int d = 0; d < ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS; d++) {
        if (!ball_binding_equals(&a->user1[d], &b->user1[d])) {
            return false;
        }
    }
    return true;
}

static void custom_config_update_dirty(void) {
    custom_config_state.dirty =
        !zmk_custom_config_equals(&custom_config_state.current, &custom_config_state.saved);
}

void zmk_custom_config_log(const char *tag, const struct zmk_custom_config *cfg) {
    LOG_INF("%s cpi_idx=%u cpi=%u scroll_div=%u scroll_div_val=%u rot_idx=%u rot_deg=%d scroll_h_rev=%u scroll_v_rev=%u scaling=%u scroll_scaling=%u scroll_layer_1=%u scroll_layer_2=%u os_mode=%u",
            tag, cfg->cpi_idx, zmk_custom_config_cpi_value_for(cfg), cfg->scroll_div,
            zmk_custom_config_scroll_div_value_for(cfg), cfg->rotation_idx,
            zmk_custom_config_rotation_deg_at(cfg->rotation_idx), cfg->scroll_h_rev,
            cfg->scroll_v_rev, cfg->scaling_mode, cfg->scroll_scaling_mode, cfg->scroll_layer_1,
            cfg->scroll_layer_2, cfg->os_mode);
    LOG_INF("%s ball sens=%u profiles=[%u %u %u %u %u %u] user1=[%u %u %u %u]", tag,
            cfg->ball_sensitivity, cfg->layer_profiles[0], cfg->layer_profiles[1],
            cfg->layer_profiles[2], cfg->layer_profiles[3], cfg->layer_profiles[4],
            cfg->layer_profiles[5], cfg->user1[0].behavior_local_id,
            cfg->user1[1].behavior_local_id, cfg->user1[2].behavior_local_id,
            cfg->user1[3].behavior_local_id);
}

void zmk_custom_config_handle_loaded_settings(struct zmk_custom_config *cfg) {
    zmk_custom_config_sanitize_layers(cfg);
    zmk_custom_config_sanitize_ball(cfg);
    custom_config = *cfg;
    custom_config_state.saved = custom_config;
    custom_config_update_dirty();
    zmk_custom_config_log("CUSTOM_CFG_LOAD", &custom_config);
    LOG_INF("Settings load complete; applying CPI");
    zmk_custom_config_apply_cpi(&custom_config);
}

void zmk_custom_config_commit_settings(bool settings_loaded) {
    zmk_custom_config_set_defaults(&custom_config_state.defaults);
    if (!settings_loaded) {
        custom_config = custom_config_state.defaults;
        custom_config_state.saved = custom_config;
        custom_config_update_dirty();
        zmk_custom_config_log("CUSTOM_CFG_DEFAULTS", &custom_config);
        LOG_INF("No settings found; applying default CPI");
        zmk_custom_config_apply_cpi(&custom_config);
    } else {
        custom_config_state.saved = custom_config;
        custom_config_update_dirty();
    }
}

uint8_t zmk_custom_config_layer_count(void) {
    return ZMK_KEYMAP_LAYERS_LEN > 0 ? ZMK_KEYMAP_LAYERS_LEN : 1;
}

const struct zmk_custom_config *zmk_custom_config_get(void) { return &custom_config; }

const struct zmk_custom_config *zmk_custom_config_saved_get(void) {
    return &custom_config_state.saved;
}

const struct zmk_custom_config *zmk_custom_config_defaults_get(void) {
    return &custom_config_state.defaults;
}

int zmk_custom_config_set(const struct zmk_custom_config *cfg) {
    return zmk_custom_config_set_with_tag(cfg, "CUSTOM_CFG_UPDATE");
}

int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag) {
    struct zmk_custom_config sanitized = *cfg;
    zmk_custom_config_sanitize_layers(&sanitized);
    zmk_custom_config_sanitize_ball(&sanitized);

    if (zmk_custom_config_equals(&custom_config, &sanitized)) {
        return 0;
    }

    uint8_t prev_cpi_idx = custom_config.cpi_idx;
    custom_config = sanitized;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    zmk_custom_config_log(tag, &custom_config);
    if (custom_config.cpi_idx != prev_cpi_idx) {
        zmk_custom_config_apply_cpi(&custom_config);
    }
    return 0;
}

uint16_t zmk_custom_config_cpi_value(void) {
    return zmk_custom_config_cpi_value_for(&custom_config);
}

uint16_t zmk_custom_config_scroll_div_value(void) {
    return zmk_custom_config_scroll_div_value_for(&custom_config);
}

int16_t zmk_custom_config_rotation_deg(void) {
    return zmk_custom_config_rotation_deg_at(custom_config.rotation_idx);
}

uint8_t zmk_custom_config_cpi_count(void) { return CUSTOM_CPI_MAX; }

uint8_t zmk_custom_config_scroll_div_count(void) { return CUSTOM_SCROLL_DIV_MAX; }

uint8_t zmk_custom_config_rotation_count(void) { return CUSTOM_ROTATION_ANGLE_COUNT; }

int16_t zmk_custom_config_rotation_deg_at(uint8_t index) {
    if (index >= CUSTOM_ROTATION_ANGLE_COUNT) {
        return 0;
    }
    return zmk_custom_config_rotation_angles[index];
}

bool zmk_custom_config_scroll_h_rev(void) { return custom_config.scroll_h_rev != 0; }
bool zmk_custom_config_scroll_v_rev(void) { return custom_config.scroll_v_rev != 0; }
bool zmk_custom_config_scaling_enabled(void) { return custom_config.scaling_mode != 0; }
bool zmk_custom_config_scroll_scaling_enabled(void) { return custom_config.scroll_scaling_mode != 0; }
uint8_t zmk_custom_config_scroll_layer_1(void) { return custom_config.scroll_layer_1; }
uint8_t zmk_custom_config_scroll_layer_2(void) { return custom_config.scroll_layer_2; }
bool zmk_custom_config_os_is_mac(void) { return custom_config.os_mode != 0; }

uint8_t zmk_custom_config_layer_profile(uint8_t layer_index) {
    if (layer_index >= ZMK_CUSTOM_CONFIG_MAX_LAYERS) {
        return ZMK_BALL_PROFILE_OFF;
    }
    uint8_t profile = custom_config.layer_profiles[layer_index];
    return profile < ZMK_BALL_PROFILE_COUNT ? profile : ZMK_BALL_PROFILE_OFF;
}

uint8_t zmk_custom_config_active_profile(void) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return zmk_custom_config_layer_profile((uint8_t)index);
}

uint8_t zmk_custom_config_ball_sensitivity(void) {
    uint8_t s = custom_config.ball_sensitivity;
    return s < ZMK_BALL_SENSITIVITY_COUNT ? s : ZMK_BALL_SENSITIVITY_NORMAL;
}

uint16_t zmk_custom_config_ball_threshold(void) {
    static const uint16_t thresholds[ZMK_BALL_SENSITIVITY_COUNT] = {
        [ZMK_BALL_SENSITIVITY_LIGHT] = 20,
        [ZMK_BALL_SENSITIVITY_NORMAL] = 40,
        [ZMK_BALL_SENSITIVITY_HEAVY] = 60,
    };
    return thresholds[zmk_custom_config_ball_sensitivity()];
}

const struct zmk_custom_config_ball_binding *zmk_custom_config_user1_binding(uint8_t direction) {
    if (direction >= ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS) {
        return NULL;
    }
    return &custom_config.user1[direction];
}

static int zmk_custom_config_save_os_mode_now(uint8_t os_mode) {
    if (custom_config_state.saved.os_mode == os_mode) {
        return 0;
    }

    struct zmk_custom_config next_saved = custom_config_state.saved;
    next_saved.os_mode = os_mode;

    int ret = zmk_custom_config_storage_save(&next_saved);
    if (ret < 0) {
        return ret;
    }

    custom_config_state.saved.os_mode = os_mode;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    zmk_custom_config_log("CUSTOM_CFG_OS_SAVE", &next_saved);
    return 0;
}

int zmk_custom_config_schedule_os_mode_save(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    uint8_t os_mode = custom_config.os_mode;

    if (custom_config_state.saved.os_mode == os_mode) {
        return 0;
    }

    custom_config_pending_os_mode = os_mode;
    int ret = k_work_reschedule(&custom_config_os_mode_save_work,
                                K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(ret, 0);
#else
    return zmk_custom_config_save_os_mode_now(custom_config.os_mode);
#endif
}

int zmk_custom_config_save(void) {
    int ret = zmk_custom_config_storage_save(&custom_config);
    if (ret < 0) {
        return ret;
    }

    custom_config_state.saved = custom_config_state.current;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    return 0;
}

int zmk_custom_config_discard(void) {
    if (!custom_config_state.dirty) {
        return 0;
    }

    uint8_t prev_cpi_idx = custom_config.cpi_idx;
    custom_config = custom_config_state.saved;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    zmk_custom_config_log("CUSTOM_CFG_DISCARD", &custom_config);
    if (custom_config.cpi_idx != prev_cpi_idx) {
        zmk_custom_config_apply_cpi(&custom_config);
    }
    return 0;
}

int zmk_custom_config_reset_settings(void) {
    int ret = zmk_custom_config_set(&custom_config_state.defaults);
    if (ret < 0) {
        return ret;
    }

    ret = zmk_custom_config_storage_delete();
    if (ret < 0) {
        return ret;
    }

    custom_config_state.saved = custom_config_state.current;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    return 0;
}

bool zmk_custom_config_check_unsaved_changes(void) { return custom_config_state.dirty; }
