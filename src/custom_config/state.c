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

static bool zmk_custom_config_equals(const struct zmk_custom_config *a,
                                     const struct zmk_custom_config *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
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
}

void zmk_custom_config_handle_loaded_settings(struct zmk_custom_config *cfg) {
    zmk_custom_config_sanitize_layers(cfg);
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
