/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <limits.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>

#define TRACKBALL_NODE DT_NODELABEL(trackball)
#define XY_CLIPPER_NODE DT_NODELABEL(xy_clipper)
#define SCROLL_TRANSFORM_NODE DT_NODELABEL(scroll_transform)
#define SENSOR_ROTATION_NODE DT_NODELABEL(sensor_rotation)
#define MOTION_SCALER_NODE DT_NODELABEL(motion_scaler)
#define SCROLL_MOTION_SCALER_NODE DT_NODELABEL(scroll_motion_scaler)
#define SCROLL_LAYER_DEFAULTS_NODE DT_NODELABEL(scroll_layer_defaults)
#define SCROLL_LAYER_GATE_NODE DT_NODELABEL(scroll_layer_gate)
#define CUSTOM_CONFIG_DEFAULTS_NODE DT_NODELABEL(custom_config_defaults)
#define BALL_PROFILE_DEFAULTS_NODE DT_NODELABEL(ball_profile_defaults)
#define MOD_TAP_NODE DT_NODELABEL(mt)
#define LAYER_TAP_NODE DT_NODELABEL(lt)

#define CUSTOM_MOD_TAP_TAPPING_TERM_DEFAULT_MS 200
#define CUSTOM_LAYER_TAP_TAPPING_TERM_DEFAULT_MS 150
#define CUSTOM_HOLD_TAP_FLAVOR_DEFAULT ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_TAP_PREFERRED
#define CUSTOM_HOLD_TAP_QUICK_TAP_DEFAULT_MS 150
#define CUSTOM_HOLD_TAP_REQUIRE_PRIOR_IDLE_DEFAULT_MS 0
#define CUSTOM_IDLE_TIMEOUT_DEFAULT_S 120
#define CUSTOM_IDLE_SLEEP_TIMEOUT_DEFAULT_S 900

static const uint16_t pointer_curve_speeds_mm_s[ZMK_POINTER_CURVE_POINT_COUNT] = {
    [ZMK_POINTER_CURVE_POINT_START] = 0,
    [ZMK_POINTER_CURVE_POINT_PRECISION] = 30,
    [ZMK_POINTER_CURVE_POINT_FAST] = 90,
    [ZMK_POINTER_CURVE_POINT_FLICK] = 200,
};

static const uint16_t pointer_gain_options[ZMK_POINTER_CURVE_POINT_COUNT][4] = {
    [ZMK_POINTER_CURVE_POINT_START] = {30, 40, 50, 60},
    [ZMK_POINTER_CURVE_POINT_PRECISION] = {60, 80, 100, 140},
    /* User-tuning steps from Stable-like through Standard to Responsive-like. */
    [ZMK_POINTER_CURVE_POINT_FAST] = {130, 180, 240, 300},
    [ZMK_POINTER_CURVE_POINT_FLICK] = {170, 240, 340, 420},
};

static const uint16_t pointer_gain_defaults[ZMK_POINTER_CURVE_POINT_COUNT] = {
    [ZMK_POINTER_CURVE_POINT_START] = 40,
    [ZMK_POINTER_CURVE_POINT_PRECISION] = 100,
    [ZMK_POINTER_CURVE_POINT_FAST] = 240,
    [ZMK_POINTER_CURVE_POINT_FLICK] = 340,
};

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

static uint16_t custom_config_default_mod_tap_tapping_term_ms(void) {
    uint16_t value = CUSTOM_MOD_TAP_TAPPING_TERM_DEFAULT_MS;

#if DT_NODE_EXISTS(MOD_TAP_NODE)
    value = DT_PROP_OR(MOD_TAP_NODE, tapping_term_ms, value);
#endif

    return value;
}

static uint16_t custom_config_default_layer_tap_tapping_term_ms(void) {
    uint16_t value = CUSTOM_LAYER_TAP_TAPPING_TERM_DEFAULT_MS;

#if DT_NODE_EXISTS(LAYER_TAP_NODE)
    value = DT_PROP_OR(LAYER_TAP_NODE, tapping_term_ms, value);
#endif

    return value;
}

static uint16_t hold_tap_timing_default(int32_t value) {
    return value <= 0 ? 0 : (uint16_t)value;
}

static uint8_t custom_config_default_mod_tap_flavor(void) {
#if DT_NODE_EXISTS(MOD_TAP_NODE)
    return (uint8_t)DT_ENUM_IDX(MOD_TAP_NODE, flavor);
#else
    return CUSTOM_HOLD_TAP_FLAVOR_DEFAULT;
#endif
}

static uint16_t custom_config_default_mod_tap_quick_tap_ms(void) {
#if DT_NODE_EXISTS(MOD_TAP_NODE)
    return hold_tap_timing_default(DT_PROP_OR(MOD_TAP_NODE, quick_tap_ms, -1));
#else
    return CUSTOM_HOLD_TAP_QUICK_TAP_DEFAULT_MS;
#endif
}

static uint16_t custom_config_default_mod_tap_require_prior_idle_ms(void) {
#if DT_NODE_EXISTS(MOD_TAP_NODE)
    return hold_tap_timing_default(DT_PROP_OR(MOD_TAP_NODE, require_prior_idle_ms, -1));
#else
    return CUSTOM_HOLD_TAP_REQUIRE_PRIOR_IDLE_DEFAULT_MS;
#endif
}

static uint8_t custom_config_default_layer_tap_flavor(void) {
#if DT_NODE_EXISTS(LAYER_TAP_NODE)
    return (uint8_t)DT_ENUM_IDX(LAYER_TAP_NODE, flavor);
#else
    return CUSTOM_HOLD_TAP_FLAVOR_DEFAULT;
#endif
}

static uint16_t custom_config_default_layer_tap_quick_tap_ms(void) {
#if DT_NODE_EXISTS(LAYER_TAP_NODE)
    return hold_tap_timing_default(DT_PROP_OR(LAYER_TAP_NODE, quick_tap_ms, -1));
#else
    return CUSTOM_HOLD_TAP_QUICK_TAP_DEFAULT_MS;
#endif
}

static uint16_t custom_config_default_layer_tap_require_prior_idle_ms(void) {
#if DT_NODE_EXISTS(LAYER_TAP_NODE)
    return hold_tap_timing_default(DT_PROP_OR(LAYER_TAP_NODE, require_prior_idle_ms, -1));
#else
    return CUSTOM_HOLD_TAP_REQUIRE_PRIOR_IDLE_DEFAULT_MS;
#endif
}

static uint16_t custom_config_default_idle_timeout_s(void) {
    uint16_t value = CUSTOM_IDLE_TIMEOUT_DEFAULT_S;

#if DT_NODE_EXISTS(CUSTOM_CONFIG_DEFAULTS_NODE)
    value = DT_PROP_OR(CUSTOM_CONFIG_DEFAULTS_NODE, idle_timeout_s, value);
#endif

    return value;
}

static uint16_t custom_config_default_idle_sleep_timeout_s(void) {
    uint16_t value = CUSTOM_IDLE_SLEEP_TIMEOUT_DEFAULT_S;

#if DT_NODE_EXISTS(CUSTOM_CONFIG_DEFAULTS_NODE)
    value = DT_PROP_OR(CUSTOM_CONFIG_DEFAULTS_NODE, idle_sleep_timeout_s, value);
#endif

    return value;
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
#if DT_NODE_HAS_PROP(BALL_PROFILE_DEFAULTS_NODE, layer_profiles)
        /* DT_PROP on an array property expands to a brace initializer, so build a
         * C array and index it with a runtime loop. DT_PROP_BY_IDX cannot be used
         * here because it token-pastes its index and thus needs a compile-time
         * constant (a loop variable produces an undeclared ..._IDX_i macro). */
        static const uint8_t ball_layer_profiles[] =
            DT_PROP(BALL_PROFILE_DEFAULTS_NODE, layer_profiles);
        for (int i = 0;
             i < (int)ARRAY_SIZE(ball_layer_profiles) && i < ZMK_CUSTOM_CONFIG_MAX_LAYERS; i++) {
            cfg->layer_profiles[i] = ball_layer_profiles[i];
        }
#endif
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

void zmk_custom_config_sanitize_scroll_scaling(struct zmk_custom_config *cfg) {
    if (cfg->scroll_scaling_mode >= ZMK_SCROLL_SCALING_MODE_COUNT) {
        cfg->scroll_scaling_mode = ZMK_SCROLL_SCALING_MODE_LINEAR;
    }
}

void zmk_custom_config_sanitize_pointer_profile(struct zmk_custom_config *cfg) {
    if (cfg->pointer_profile >= ZMK_POINTER_PROFILE_COUNT) {
        cfg->pointer_profile = ZMK_POINTER_PROFILE_STANDARD;
    }
}

uint16_t zmk_custom_config_pointer_curve_speed_mm_s(uint8_t point) {
    return point < ZMK_POINTER_CURVE_POINT_COUNT ? pointer_curve_speeds_mm_s[point] : 0;
}

uint8_t zmk_custom_config_pointer_gain_option_count(uint8_t point) {
    return point < ZMK_POINTER_CURVE_POINT_COUNT ? ARRAY_SIZE(pointer_gain_options[point]) : 0;
}

uint16_t zmk_custom_config_pointer_gain_option_at(uint8_t point, uint8_t option) {
    return point < ZMK_POINTER_CURVE_POINT_COUNT &&
                   option < ARRAY_SIZE(pointer_gain_options[point])
               ? pointer_gain_options[point][option]
               : 0;
}

bool zmk_custom_config_pointer_curve_is_valid(const struct zmk_custom_config *cfg) {
    uint16_t previous = 0;

    for (uint8_t point = 0; point < ZMK_POINTER_CURVE_POINT_COUNT; point++) {
        uint16_t gain = cfg->pointer_custom_gain_percent[point];
        bool allowed = false;
        for (uint8_t option = 0; option < ARRAY_SIZE(pointer_gain_options[point]); option++) {
            if (gain == pointer_gain_options[point][option]) {
                allowed = true;
                break;
            }
        }
        if (!allowed || (point > 0 && gain < previous)) {
            return false;
        }
        previous = gain;
    }

    return true;
}

void zmk_custom_config_sanitize_pointer_curve(struct zmk_custom_config *cfg) {
    if (zmk_custom_config_pointer_curve_is_valid(cfg)) {
        return;
    }

    for (uint8_t point = 0; point < ZMK_POINTER_CURVE_POINT_COUNT; point++) {
        cfg->pointer_custom_gain_percent[point] = pointer_gain_defaults[point];
    }
}

static uint16_t sanitize_stepped_value(uint16_t value, uint16_t min, uint16_t max, uint16_t step,
                                       bool allow_disabled) {
    if (allow_disabled && value == 0) {
        return 0;
    }

    value = CLAMP(value, min, max);
    return min + ((value - min) / step) * step;
}

void zmk_custom_config_sanitize_timing(struct zmk_custom_config *cfg) {
    if (cfg->mod_tap_flavor >= ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_COUNT) {
        cfg->mod_tap_flavor = custom_config_default_mod_tap_flavor();
    }
    if (cfg->layer_tap_flavor >= ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_COUNT) {
        cfg->layer_tap_flavor = custom_config_default_layer_tap_flavor();
    }
    cfg->mod_tap_tapping_term_ms =
        sanitize_stepped_value(cfg->mod_tap_tapping_term_ms,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_MIN_MS,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_MAX_MS,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_STEP_MS, false);
    cfg->layer_tap_tapping_term_ms =
        sanitize_stepped_value(cfg->layer_tap_tapping_term_ms,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_MIN_MS,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_MAX_MS,
                               ZMK_CUSTOM_CONFIG_TAPPING_TERM_STEP_MS, false);
    cfg->mod_tap_quick_tap_ms =
        sanitize_stepped_value(cfg->mod_tap_quick_tap_ms,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MIN_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MAX_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_STEP_MS, true);
    cfg->mod_tap_require_prior_idle_ms =
        sanitize_stepped_value(cfg->mod_tap_require_prior_idle_ms,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MIN_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MAX_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_STEP_MS, true);
    cfg->layer_tap_quick_tap_ms =
        sanitize_stepped_value(cfg->layer_tap_quick_tap_ms,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MIN_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MAX_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_STEP_MS, true);
    cfg->layer_tap_require_prior_idle_ms =
        sanitize_stepped_value(cfg->layer_tap_require_prior_idle_ms,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MIN_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MAX_MS,
                               ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_STEP_MS, true);
    cfg->idle_timeout_s =
        sanitize_stepped_value(cfg->idle_timeout_s, ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_MIN_S,
                               ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_MAX_S,
                               ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_STEP_S, true);
    cfg->idle_sleep_timeout_s =
        sanitize_stepped_value(cfg->idle_sleep_timeout_s,
                               ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_MIN_S,
                               ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_MAX_S,
                               ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_STEP_S, true);

    if (cfg->idle_timeout_s != 0 && cfg->idle_sleep_timeout_s != 0 &&
        cfg->idle_sleep_timeout_s < cfg->idle_timeout_s) {
        uint16_t step = ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_STEP_S;
        cfg->idle_sleep_timeout_s = ((cfg->idle_timeout_s + step - 1) / step) * step;
    }
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
    uint16_t mod_tap_tapping_term_ms = custom_config_default_mod_tap_tapping_term_ms();
    uint16_t layer_tap_tapping_term_ms = custom_config_default_layer_tap_tapping_term_ms();
    uint16_t idle_timeout_s = custom_config_default_idle_timeout_s();
    uint16_t idle_sleep_timeout_s = custom_config_default_idle_sleep_timeout_s();
    uint8_t mod_tap_flavor = custom_config_default_mod_tap_flavor();
    uint16_t mod_tap_quick_tap_ms = custom_config_default_mod_tap_quick_tap_ms();
    uint16_t mod_tap_require_prior_idle_ms =
        custom_config_default_mod_tap_require_prior_idle_ms();
    uint8_t layer_tap_flavor = custom_config_default_layer_tap_flavor();
    uint16_t layer_tap_quick_tap_ms = custom_config_default_layer_tap_quick_tap_ms();
    uint16_t layer_tap_require_prior_idle_ms =
        custom_config_default_layer_tap_require_prior_idle_ms();

#if DT_NODE_EXISTS(TRACKBALL_NODE)
    {
        int32_t cpi = DT_PROP(TRACKBALL_NODE, cpi);
        cpi_idx = zmk_custom_config_axis_value_to_idx(zmk_custom_config_cpi_axis(), cpi);
    }
#endif

#if DT_NODE_EXISTS(SCROLL_TRANSFORM_NODE)
    {
        int32_t threshold = DT_PROP(SCROLL_TRANSFORM_NODE, threshold);
        scroll_div =
            zmk_custom_config_axis_value_to_idx(zmk_custom_config_scroll_div_axis(), threshold);
        scroll_h_rev = DT_PROP(SCROLL_TRANSFORM_NODE, invert_x) ? 1 : 0;
        scroll_v_rev = DT_PROP(SCROLL_TRANSFORM_NODE, invert_y) ? 1 : 0;
        scroll_scaling_mode =
            DT_PROP(SCROLL_TRANSFORM_NODE, scaling_mode) ? ZMK_SCROLL_SCALING_MODE_ADAPTIVE
                                                        : ZMK_SCROLL_SCALING_MODE_LINEAR;
    }
#elif DT_NODE_EXISTS(XY_CLIPPER_NODE)
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
    cfg->mod_tap_tapping_term_ms = mod_tap_tapping_term_ms;
    cfg->layer_tap_tapping_term_ms = layer_tap_tapping_term_ms;
    cfg->idle_timeout_s = idle_timeout_s;
    cfg->idle_sleep_timeout_s = idle_sleep_timeout_s;
    cfg->mod_tap_flavor = mod_tap_flavor;
    cfg->mod_tap_quick_tap_ms = mod_tap_quick_tap_ms;
    cfg->mod_tap_require_prior_idle_ms = mod_tap_require_prior_idle_ms;
    cfg->layer_tap_flavor = layer_tap_flavor;
    cfg->layer_tap_quick_tap_ms = layer_tap_quick_tap_ms;
    cfg->layer_tap_require_prior_idle_ms = layer_tap_require_prior_idle_ms;
    cfg->pointer_profile = ZMK_POINTER_PROFILE_STANDARD;
    for (uint8_t point = 0; point < ZMK_POINTER_CURVE_POINT_COUNT; point++) {
        cfg->pointer_custom_gain_percent[point] = pointer_gain_defaults[point];
    }
    custom_config_default_ball(cfg);
    zmk_custom_config_sanitize_layers(cfg);
    zmk_custom_config_sanitize_pointer_profile(cfg);
    zmk_custom_config_sanitize_pointer_curve(cfg);
    zmk_custom_config_sanitize_scroll_scaling(cfg);
    zmk_custom_config_sanitize_timing(cfg);
    zmk_custom_config_sanitize_ball(cfg);
}
