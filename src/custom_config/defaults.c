/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <limits.h>
#include <stdint.h>

#include <zephyr/devicetree.h>

#define TRACKBALL_NODE DT_NODELABEL(trackball)
#define XY_CLIPPER_NODE DT_NODELABEL(xy_clipper)
#define SENSOR_ROTATION_NODE DT_NODELABEL(sensor_rotation)
#define MOTION_SCALER_NODE DT_NODELABEL(motion_scaler)
#define SCROLL_MOTION_SCALER_NODE DT_NODELABEL(scroll_motion_scaler)
#define SCROLL_LAYER_DEFAULTS_NODE DT_NODELABEL(scroll_layer_defaults)
#define SCROLL_LAYER_GATE_NODE DT_NODELABEL(scroll_layer_gate)
#define CUSTOM_CONFIG_DEFAULTS_NODE DT_NODELABEL(custom_config_defaults)
#define BALL_PROFILE_DEFAULTS_NODE DT_NODELABEL(ball_profile_defaults)

const int16_t zmk_custom_config_rotation_angles[CUSTOM_ROTATION_ANGLE_COUNT] = {
    -70, -65, -60, -55, -50, -45, -40, -35, -30, -25,
    -20, -15, -10, -5,  0,   5,   10,  15,  20,  25,
    30,  35,  40,  45,  50,  55,  60,  65,  70,
};

static uint8_t rotation_index_from_deg(int32_t deg) {
    int32_t best_diff = INT32_MAX;
    uint8_t best_idx = CUSTOM_ROTATION_DEFAULT;

    for (uint8_t i = 0; i < CUSTOM_ROTATION_ANGLE_COUNT; i++) {
        int32_t diff = deg - zmk_custom_config_rotation_angles[i];
        if (diff < 0) {
            diff = -diff;
        }
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void custom_config_default_scroll_layers(uint8_t *layer_1, uint8_t *layer_2) {
    uint8_t default_layer_1 = 0;
    uint8_t default_layer_2 = 0;

#if DT_NODE_EXISTS(SCROLL_LAYER_DEFAULTS_NODE)
    {
        int len = DT_PROP_LEN(SCROLL_LAYER_DEFAULTS_NODE, layers);
        if (len > 0) {
            default_layer_1 = DT_PROP_BY_IDX(SCROLL_LAYER_DEFAULTS_NODE, layers, 0);
        }
        if (len > 1) {
            default_layer_2 = DT_PROP_BY_IDX(SCROLL_LAYER_DEFAULTS_NODE, layers, 1);
        }
    }
#elif DT_NODE_EXISTS(SCROLL_LAYER_GATE_NODE)
    {
        default_layer_1 = DT_PROP_OR(SCROLL_LAYER_GATE_NODE, layer_1, default_layer_1);
        default_layer_2 = DT_PROP_OR(SCROLL_LAYER_GATE_NODE, layer_2, default_layer_2);
    }
#endif

    *layer_1 = default_layer_1;
    *layer_2 = default_layer_2;
}

static uint8_t custom_config_default_os_mode(void) {
    uint8_t os_mode = 0;

#if DT_NODE_EXISTS(CUSTOM_CONFIG_DEFAULTS_NODE)
    os_mode = DT_PROP(CUSTOM_CONFIG_DEFAULTS_NODE, os_mode) ? 1 : 0;
#endif

    return os_mode;
}

static void custom_config_default_ball(struct zmk_custom_config *cfg) {
    for (int i = 0; i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
        cfg->layer_profiles[i] = ZMK_BALL_PROFILE_OFF;
    }
    cfg->ball_sensitivity = ZMK_BALL_SENSITIVITY_NORMAL;
    for (int d = 0; d < ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS; d++) {
        cfg->user1[d] = (struct zmk_custom_config_ball_binding){0};
    }

#if DT_NODE_EXISTS(BALL_PROFILE_DEFAULTS_NODE)
    {
        int len = DT_PROP_LEN_OR(BALL_PROFILE_DEFAULTS_NODE, layer_profiles, 0);
        for (int i = 0; i < len && i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
            cfg->layer_profiles[i] =
                (uint8_t)DT_PROP_BY_IDX(BALL_PROFILE_DEFAULTS_NODE, layer_profiles, i);
        }
#if DT_NODE_HAS_PROP(BALL_PROFILE_DEFAULTS_NODE, sensitivity)
        cfg->ball_sensitivity = (uint8_t)DT_PROP(BALL_PROFILE_DEFAULTS_NODE, sensitivity);
#endif
    }
#endif
}

void zmk_custom_config_sanitize_ball(struct zmk_custom_config *cfg) {
    for (int i = 0; i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
        if (cfg->layer_profiles[i] >= ZMK_BALL_PROFILE_COUNT) {
            cfg->layer_profiles[i] = ZMK_BALL_PROFILE_OFF;
        }
    }
    if (cfg->ball_sensitivity >= ZMK_BALL_SENSITIVITY_COUNT) {
        cfg->ball_sensitivity = ZMK_BALL_SENSITIVITY_NORMAL;
    }
}

void zmk_custom_config_sanitize_layers(struct zmk_custom_config *cfg) {
    uint8_t layer_count = zmk_custom_config_layer_count();
    uint8_t default_layer_1 = 0;
    uint8_t default_layer_2 = 0;

    custom_config_default_scroll_layers(&default_layer_1, &default_layer_2);

    cfg->scroll_layer_1 = default_layer_1 % layer_count;
    cfg->scroll_layer_2 %= layer_count;
}

void zmk_custom_config_set_defaults(struct zmk_custom_config *cfg) {
    uint8_t cpi_idx = CUSTOM_CPI_DEFAULT;
    uint8_t scroll_div = CUSTOM_SCROLL_DIV_DEFAULT;
    uint8_t rotation_idx = CUSTOM_ROTATION_DEFAULT;
    uint8_t scroll_h_rev = 1;
    uint8_t scroll_v_rev = 0;
    uint8_t scaling_mode = 0;
    uint8_t scroll_scaling_mode = 0;
    uint8_t scroll_layer_1 = 0;
    uint8_t scroll_layer_2 = 0;
    uint8_t os_mode = custom_config_default_os_mode();

#if DT_NODE_EXISTS(TRACKBALL_NODE)
    {
        int32_t cpi = DT_PROP(TRACKBALL_NODE, cpi);
        cpi_idx = zmk_custom_config_axis_value_to_idx(zmk_custom_config_cpi_axis(), cpi);
    }
#endif

#if DT_NODE_EXISTS(XY_CLIPPER_NODE)
    {
        int32_t threshold = DT_PROP(XY_CLIPPER_NODE, threshold);
        scroll_div =
            zmk_custom_config_axis_value_to_idx(zmk_custom_config_scroll_div_axis(), threshold);
        scroll_h_rev = DT_PROP(XY_CLIPPER_NODE, invert_x) ? 1 : 0;
        scroll_v_rev = DT_PROP(XY_CLIPPER_NODE, invert_y) ? 1 : 0;
    }
#endif

#if DT_NODE_EXISTS(SENSOR_ROTATION_NODE)
    {
        int32_t deg = DT_PROP(SENSOR_ROTATION_NODE, rotation_angle);
        rotation_idx = rotation_index_from_deg(deg);
    }
#endif

#if DT_NODE_EXISTS(MOTION_SCALER_NODE)
    scaling_mode = DT_PROP(MOTION_SCALER_NODE, scaling_mode) ? 1 : 0;
#endif
#if DT_NODE_EXISTS(SCROLL_MOTION_SCALER_NODE)
    scroll_scaling_mode = DT_PROP(SCROLL_MOTION_SCALER_NODE, scaling_mode) ? 1 : 0;
#endif

    custom_config_default_scroll_layers(&scroll_layer_1, &scroll_layer_2);

    cfg->cpi_idx = cpi_idx;
    cfg->scroll_div = scroll_div;
    cfg->rotation_idx = rotation_idx;
    cfg->scroll_h_rev = scroll_h_rev;
    cfg->scroll_v_rev = scroll_v_rev;
    cfg->scaling_mode = scaling_mode;
    cfg->scroll_scaling_mode = scroll_scaling_mode;
    cfg->scroll_layer_1 = scroll_layer_1;
    cfg->scroll_layer_2 = scroll_layer_2;
    cfg->os_mode = os_mode;
    custom_config_default_ball(cfg);
    zmk_custom_config_sanitize_layers(cfg);
    zmk_custom_config_sanitize_ball(cfg);
}
