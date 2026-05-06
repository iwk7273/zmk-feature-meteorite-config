/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/devicetree.h>

#include <dt-bindings/zmk/custom_config.h>
#include <zmk/custom_feature.h>
#include <zmk/keymap.h>

#define CUSTOM_CPI_DEFAULT 4
#define CUSTOM_CPI_MAX 16
#define CUSTOM_CPI_STEP 200
#define CUSTOM_SCROLL_DIV_DEFAULT 3
#define CUSTOM_SCROLL_DIV_MAX 16
#define CUSTOM_ROTATION_DEFAULT 20

static const int16_t rotation_angles[] = {-70, -65, -60, -55, -50, -45, -40, -35,
                                          -30, -25, -20, -15, -10, -5,  0,   5,
                                          10,  15,  20,  25,  30,  35,  40,  45,
                                          50,  55,  60,  65,  70};
#define ROTATION_ANGLE_COUNT (sizeof(rotation_angles) / sizeof(rotation_angles[0]))

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct meteorite_config_state {
    struct zmk_custom_config current;
    struct zmk_custom_config saved;
    struct zmk_custom_config defaults;
    bool dirty;
};

static struct meteorite_config_state custom_config_state;
#define custom_config (custom_config_state.current)

#if DT_NODE_EXISTS(DT_NODELABEL(trackball))
#define TRACKBALL_NODE DT_NODELABEL(trackball)
#define HAVE_TRACKBALL_NODE 1
#else
#define HAVE_TRACKBALL_NODE 0
#endif

#ifndef PMW3610_ATTR_CPI
/* Keep in sync with zmk-pmw3610-driver/src/pmw3610.h */
#define PMW3610_ATTR_CPI 0
#endif

#define XY_CLIPPER_NODE DT_NODELABEL(xy_clipper)
#define SENSOR_ROTATION_NODE DT_NODELABEL(sensor_rotation)
#define MOTION_SCALER_NODE DT_NODELABEL(motion_scaler)
#define SCROLL_MOTION_SCALER_NODE DT_NODELABEL(scroll_motion_scaler)
#define SCROLL_LAYER_DEFAULTS_NODE DT_NODELABEL(scroll_layer_defaults)
#define SCROLL_LAYER_GATE_NODE DT_NODELABEL(scroll_layer_gate)
#define CUSTOM_CONFIG_DEFAULTS_NODE DT_NODELABEL(custom_config_defaults)
#define CUSTOM_CONFIG_SETTINGS_KEY "custom_config/state"

#if IS_ENABLED(CONFIG_SETTINGS)
static bool settings_init;

static int custom_feature_save_state(void) {
    int ret = settings_save_one(CUSTOM_CONFIG_SETTINGS_KEY, &custom_config, sizeof(custom_config));
    if (ret < 0) {
        LOG_WRN("Failed to save custom config (%d)", ret);
    } else {
        LOG_INF("Saved custom config");
    }
    return ret;
}

static int custom_feature_delete_state(void) {
    int ret = settings_delete(CUSTOM_CONFIG_SETTINGS_KEY);
    if (ret < 0) {
        LOG_WRN("Failed to delete custom config settings (%d)", ret);
    } else {
        LOG_INF("Deleted custom config settings");
    }
    return ret;
}
#else
static inline int custom_feature_save_state(void) { return 0; }
static inline int custom_feature_delete_state(void) { return 0; }
#endif

__weak void zmk_custom_config_changed(const struct zmk_custom_config *cfg) { ARG_UNUSED(cfg); }

static bool zmk_custom_config_equals(const struct zmk_custom_config *a,
                                     const struct zmk_custom_config *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

static void custom_config_update_dirty(void) {
    custom_config_state.dirty =
        !zmk_custom_config_equals(&custom_config_state.current, &custom_config_state.saved);
}

static void zmk_custom_config_log(const char *tag, const struct zmk_custom_config *cfg) {
    LOG_INF("%s cpi_idx=%u cpi=%u scroll_div=%u scroll_div_val=%u rot_idx=%u rot_deg=%d scroll_h_rev=%u scroll_v_rev=%u scaling=%u scroll_scaling=%u scroll_layer_1=%u scroll_layer_2=%u os_mode=%u",
            tag, cfg->cpi_idx, zmk_custom_config_cpi_value(), cfg->scroll_div,
            zmk_custom_config_scroll_div_value(), cfg->rotation_idx,
            zmk_custom_config_rotation_deg(), cfg->scroll_h_rev, cfg->scroll_v_rev,
            cfg->scaling_mode, cfg->scroll_scaling_mode, cfg->scroll_layer_1, cfg->scroll_layer_2,
            cfg->os_mode);
}

static const char *custom_config_op_name(uint8_t op) {
    switch (op) {
    case C_CPI_UP:
        return "C_CPI_UP";
    case C_CPI_DN:
        return "C_CPI_DN";
    case C_SDIV_UP:
        return "C_SDIV_UP";
    case C_SDIV_DN:
        return "C_SDIV_DN";
    case C_ROT_UP:
        return "C_ROT_UP";
    case C_ROT_DN:
        return "C_ROT_DN";
    case C_SCALE_TOG:
        return "C_SCALE_TOG";
    case C_SCRH_TOG:
        return "C_SCRH_TOG";
    case C_SCRV_TOG:
        return "C_SCRV_TOG";
    case C_SCRL1_UP:
        return "C_SCRL1_UP";
    case C_SCRL2_UP:
        return "C_SCRL2_UP";
    case C_SCRL_SCALE_TOG:
        return "C_SCRL_SCALE_TOG";
    case C_OS_TOG:
        return "C_OS_TOG";
    case C_OS_WIN:
        return "C_OS_WIN";
    case C_OS_MAC:
        return "C_OS_MAC";
    case C_RESET:
        return "C_RESET";
    case C_SAVE:
        return "C_SAVE";
    default:
        return "CUSTOM_CFG_UNKNOWN";
    }
}

static uint8_t clamp_u8(int32_t v, uint8_t max) {
    if (v < 0) {
        return 0;
    }
    if (v >= max) {
        return (uint8_t)(max - 1);
    }
    return (uint8_t)v;
}

static uint8_t rotation_index_from_deg(int32_t deg) {
    int32_t best_diff = INT32_MAX;
    uint8_t best_idx = CUSTOM_ROTATION_DEFAULT;

    for (uint8_t i = 0; i < ROTATION_ANGLE_COUNT; i++) {
        int32_t diff = deg - rotation_angles[i];
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

uint8_t zmk_custom_config_layer_count(void) {
    return ZMK_KEYMAP_LAYERS_LEN > 0 ? ZMK_KEYMAP_LAYERS_LEN : 1;
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

static void custom_config_sanitize_layers(struct zmk_custom_config *cfg) {
    uint8_t layer_count = zmk_custom_config_layer_count();
    uint8_t default_layer_1 = 0;
    uint8_t default_layer_2 = 0;

    custom_config_default_scroll_layers(&default_layer_1, &default_layer_2);

    cfg->scroll_layer_1 = default_layer_1 % layer_count;
    cfg->scroll_layer_2 %= layer_count;
}

static void zmk_custom_config_apply_cpi(const struct zmk_custom_config *cfg) {
#if HAVE_TRACKBALL_NODE
    const struct device *dev = DEVICE_DT_GET(TRACKBALL_NODE);
    if (!device_is_ready(dev)) {
        LOG_WRN("CPI apply skipped: trackball device not ready (cpi=%u)",
                zmk_custom_config_cpi_value());
        return;
    }

    struct sensor_value val = {
        .val1 = zmk_custom_config_cpi_value(),
        .val2 = 0,
    };
    int ret = sensor_attr_set(dev, SENSOR_CHAN_ALL, PMW3610_ATTR_CPI, &val);
    if (ret < 0) {
        LOG_WRN("Failed to set CPI %u (%d)", val.val1, ret);
    } else {
        LOG_INF("Applied CPI %u", val.val1);
    }
#else
    ARG_UNUSED(cfg);
    LOG_WRN("CPI apply skipped: trackball node not present");
#endif
}

static int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag);

static void zmk_custom_config_set_defaults(struct zmk_custom_config *cfg) {
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
        int32_t idx = ((cpi + (CUSTOM_CPI_STEP / 2)) / CUSTOM_CPI_STEP) - 1;
        cpi_idx = clamp_u8(idx, CUSTOM_CPI_MAX);
    }
#endif

#if DT_NODE_EXISTS(XY_CLIPPER_NODE)
    {
        int32_t threshold = DT_PROP(XY_CLIPPER_NODE, threshold);
        int32_t idx = ((threshold + 2) / 5) - 1;
        scroll_div = clamp_u8(idx, CUSTOM_SCROLL_DIV_MAX);
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
    custom_config_sanitize_layers(cfg);
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

static int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag) {
    struct zmk_custom_config sanitized = *cfg;
    custom_config_sanitize_layers(&sanitized);

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
    return (custom_config.cpi_idx + 1) * CUSTOM_CPI_STEP;
}

uint16_t zmk_custom_config_scroll_div_value(void) {
    return (custom_config.scroll_div + 1) * 5;
}

int16_t zmk_custom_config_rotation_deg(void) {
    if (custom_config.rotation_idx >= ROTATION_ANGLE_COUNT) {
        return 0;
    }
    return rotation_angles[custom_config.rotation_idx];
}

uint8_t zmk_custom_config_cpi_count(void) { return CUSTOM_CPI_MAX; }

uint8_t zmk_custom_config_scroll_div_count(void) { return CUSTOM_SCROLL_DIV_MAX; }

uint8_t zmk_custom_config_rotation_count(void) { return ROTATION_ANGLE_COUNT; }

int16_t zmk_custom_config_rotation_deg_at(uint8_t index) {
    if (index >= ROTATION_ANGLE_COUNT) {
        return 0;
    }
    return rotation_angles[index];
}

bool zmk_custom_config_scroll_h_rev(void) { return custom_config.scroll_h_rev != 0; }
bool zmk_custom_config_scroll_v_rev(void) { return custom_config.scroll_v_rev != 0; }
bool zmk_custom_config_scaling_enabled(void) { return custom_config.scaling_mode != 0; }
bool zmk_custom_config_scroll_scaling_enabled(void) { return custom_config.scroll_scaling_mode != 0; }
uint8_t zmk_custom_config_scroll_layer_1(void) { return custom_config.scroll_layer_1; }
uint8_t zmk_custom_config_scroll_layer_2(void) { return custom_config.scroll_layer_2; }
bool zmk_custom_config_os_is_mac(void) { return custom_config.os_mode != 0; }

static void custom_config_wrap_inc(uint8_t *value, uint8_t max) {
    *value = (*value + 1) % max;
}

static void custom_config_wrap_dec(uint8_t *value, uint8_t max) {
    *value = (*value + max - 1) % max;
}

int zmk_custom_config_apply_op(uint8_t op) {
    struct zmk_custom_config next = custom_config;

    switch (op) {
    case C_CPI_UP:
        custom_config_wrap_inc(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case C_CPI_DN:
        custom_config_wrap_dec(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case C_SDIV_UP:
        custom_config_wrap_inc(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case C_SDIV_DN:
        custom_config_wrap_dec(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case C_ROT_UP:
        custom_config_wrap_inc(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case C_ROT_DN:
        custom_config_wrap_dec(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case C_SCALE_TOG:
        next.scaling_mode ^= 1;
        break;
    case C_SCRL_SCALE_TOG:
        next.scroll_scaling_mode ^= 1;
        break;
    case C_SCRH_TOG:
        next.scroll_h_rev ^= 1;
        break;
    case C_SCRV_TOG:
        next.scroll_v_rev ^= 1;
        break;
    case C_SCRL1_UP:
        /* scroll_layer_1 is fixed to the default */
        break;
    case C_SCRL2_UP:
        custom_config_wrap_inc(&next.scroll_layer_2, zmk_custom_config_layer_count());
        break;
    case C_OS_TOG:
        next.os_mode ^= 1;
        break;
    case C_OS_WIN:
        next.os_mode = 0;
        break;
    case C_OS_MAC:
        next.os_mode = 1;
        break;
    case C_RESET:
        zmk_custom_config_set_defaults(&next);
        break;
    case C_SAVE:
        zmk_custom_config_log("C_SAVE", &custom_config);
        return zmk_custom_config_save();
    default:
        return -ENOTSUP;
    }

    custom_config_sanitize_layers(&next);
    int ret = zmk_custom_config_set_with_tag(&next, custom_config_op_name(op));
    if (ret < 0) {
        return ret;
    }
    return ret;
}

int zmk_custom_config_save(void) {
    int ret = custom_feature_save_state();
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

    ret = custom_feature_delete_state();
    if (ret < 0) {
        return ret;
    }

    custom_config_state.saved = custom_config_state.current;
    custom_config_update_dirty();
    zmk_custom_config_changed(&custom_config);
    return 0;
}

bool zmk_custom_config_check_unsaved_changes(void) { return custom_config_state.dirty; }

#if IS_ENABLED(CONFIG_SETTINGS)
static int custom_feature_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                       void *cb_arg) {
    if (!settings_name_steq(name, "state", NULL)) {
        return -ENOENT;
    }

    const size_t size_v2 = sizeof(custom_config);
    const size_t size_v2_no_os = size_v2 - 1;
    const size_t size_v2_no_scroll_scaling = size_v2 - 2;

    if (len != size_v2 && len != size_v2_no_os && len != size_v2_no_scroll_scaling) {
        return -EINVAL;
    }

    memset(&custom_config, 0, sizeof(custom_config));
    bool has_os = false;
    bool has_scroll_scaling = false;
    int rc = 0;

    if (len == size_v2) {
        rc = read_cb(cb_arg, &custom_config, len);
        if (rc < 0) {
            return rc;
        }
        has_os = true;
        has_scroll_scaling = true;
    } else {
        rc = read_cb(cb_arg, &custom_config, len);
        if (rc < 0) {
            return rc;
        }
        has_os = false;
        has_scroll_scaling = (len > size_v2_no_scroll_scaling);
    }

    if (rc >= 0) {
        if (!has_scroll_scaling) {
#if DT_NODE_EXISTS(SCROLL_MOTION_SCALER_NODE)
            custom_config.scroll_scaling_mode =
                DT_PROP(SCROLL_MOTION_SCALER_NODE, scaling_mode) ? 1 : 0;
#else
            custom_config.scroll_scaling_mode = 0;
#endif
        }
        if (!has_os) {
            custom_config.os_mode = custom_config_default_os_mode();
        }
        custom_config_sanitize_layers(&custom_config);
        custom_config_state.saved = custom_config;
        custom_config_update_dirty();
        settings_init = true;
        zmk_custom_config_changed(&custom_config);
        zmk_custom_config_log("CUSTOM_CFG_LOAD", &custom_config);
        LOG_INF("Settings load complete; applying CPI");
        zmk_custom_config_apply_cpi(&custom_config);
        return 0;
    }

    return rc;
}

static int custom_feature_settings_commit(void) {
    zmk_custom_config_set_defaults(&custom_config_state.defaults);
    if (!settings_init) {
        custom_config = custom_config_state.defaults;
        custom_config_state.saved = custom_config;
        custom_config_update_dirty();
        zmk_custom_config_changed(&custom_config);
        zmk_custom_config_log("CUSTOM_CFG_DEFAULTS", &custom_config);
        LOG_INF("No settings found; applying default CPI");
        zmk_custom_config_apply_cpi(&custom_config);
    } else {
        custom_config_state.saved = custom_config;
        custom_config_update_dirty();
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_feature, "custom_config", NULL,
                               custom_feature_settings_set, custom_feature_settings_commit, NULL);
#endif


