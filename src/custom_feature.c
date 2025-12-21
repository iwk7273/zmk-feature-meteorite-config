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

#define CUSTOM_CPI_DEFAULT 8
#define CUSTOM_CPI_MAX 31
#define CUSTOM_SCROLL_DIV_DEFAULT 3
#define CUSTOM_SCROLL_DIV_MAX 16
#define CUSTOM_ROTATION_DEFAULT 20

static const int16_t rotation_angles[] = {-70, -65, -60, -55, -50, -45, -40, -35,
                                          -30, -25, -20, -15, -10, -5,  0,   5,
                                          10,  15,  20,  25,  30,  35,  40,  45,
                                          50,  55,  60,  65,  70};
#define ROTATION_ANGLE_COUNT (sizeof(rotation_angles) / sizeof(rotation_angles[0]))

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_custom_config custom_config;

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

#if IS_ENABLED(CONFIG_SETTINGS)
static bool settings_init;

static int custom_feature_save_state(void) {
    return settings_save_one("custom_config/state", &custom_config, sizeof(custom_config));
}
#else
static inline int custom_feature_save_state(void) { return 0; }
#endif

__weak void zmk_custom_config_changed(const struct zmk_custom_config *cfg) { ARG_UNUSED(cfg); }

static void zmk_custom_config_log(const char *tag, const struct zmk_custom_config *cfg) {
    LOG_INF("%s cpi_idx=%u cpi=%u scroll_div=%u scroll_div_val=%u rot_idx=%u rot_deg=%d scroll_h_rev=%u scroll_v_rev=%u scaling=%u",
            tag, cfg->cpi_idx, zmk_custom_config_cpi_value(), cfg->scroll_div,
            zmk_custom_config_scroll_div_value(), cfg->rotation_idx,
            zmk_custom_config_rotation_deg(), cfg->scroll_h_rev, cfg->scroll_v_rev,
            cfg->scaling_mode);
}

static const char *custom_config_op_name(uint8_t op) {
    switch (op) {
    case CCFG_CPI_UP:
        return "CCFG_CPI_UP";
    case CCFG_CPI_DN:
        return "CCFG_CPI_DN";
    case CCFG_SDIV_UP:
        return "CCFG_SDIV_UP";
    case CCFG_SDIV_DN:
        return "CCFG_SDIV_DN";
    case CCFG_ROT_UP:
        return "CCFG_ROT_UP";
    case CCFG_ROT_DN:
        return "CCFG_ROT_DN";
    case CCFG_SCALE_TOG:
        return "CCFG_SCALE_TOG";
    case CCFG_SCRH_TOG:
        return "CCFG_SCRH_TOG";
    case CCFG_SCRV_TOG:
        return "CCFG_SCRV_TOG";
    case CCFG_RESET:
        return "CCFG_RESET";
    case CCFG_SAVE:
        return "CCFG_SAVE";
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

static void zmk_custom_config_apply_cpi(const struct zmk_custom_config *cfg) {
#if HAVE_TRACKBALL_NODE
    const struct device *dev = DEVICE_DT_GET(TRACKBALL_NODE);
    if (!device_is_ready(dev)) {
        LOG_WRN("CPI apply skipped: trackball device not ready");
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

#if DT_NODE_EXISTS(TRACKBALL_NODE)
    {
        int32_t cpi = DT_PROP(TRACKBALL_NODE, cpi);
        int32_t idx = ((cpi + 50) / 100) - 2;
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

    cfg->cpi_idx = cpi_idx;
    cfg->scroll_div = scroll_div;
    cfg->rotation_idx = rotation_idx;
    cfg->scroll_h_rev = scroll_h_rev;
    cfg->scroll_v_rev = scroll_v_rev;
    cfg->scaling_mode = scaling_mode;
}

static bool zmk_custom_config_equals(const struct zmk_custom_config *a,
                                     const struct zmk_custom_config *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

const struct zmk_custom_config *zmk_custom_config_get(void) { return &custom_config; }

int zmk_custom_config_set(const struct zmk_custom_config *cfg) {
    return zmk_custom_config_set_with_tag(cfg, "CUSTOM_CFG_UPDATE");
}

static int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag) {
    if (zmk_custom_config_equals(&custom_config, cfg)) {
        return 0;
    }

    uint8_t prev_cpi_idx = custom_config.cpi_idx;
    custom_config = *cfg;
    zmk_custom_config_changed(&custom_config);
    zmk_custom_config_log(tag, &custom_config);
    if (custom_config.cpi_idx != prev_cpi_idx) {
        zmk_custom_config_apply_cpi(&custom_config);
    }
    return 0;
}

uint16_t zmk_custom_config_cpi_value(void) { return (custom_config.cpi_idx + 2) * 100; }

uint16_t zmk_custom_config_scroll_div_value(void) {
    return (custom_config.scroll_div + 1) * 5;
}

int16_t zmk_custom_config_rotation_deg(void) {
    if (custom_config.rotation_idx >= ROTATION_ANGLE_COUNT) {
        return 0;
    }
    return rotation_angles[custom_config.rotation_idx];
}

bool zmk_custom_config_scroll_h_rev(void) { return custom_config.scroll_h_rev != 0; }
bool zmk_custom_config_scroll_v_rev(void) { return custom_config.scroll_v_rev != 0; }
bool zmk_custom_config_scaling_enabled(void) { return custom_config.scaling_mode != 0; }

static void custom_config_wrap_inc(uint8_t *value, uint8_t max) {
    *value = (*value + 1) % max;
}

static void custom_config_wrap_dec(uint8_t *value, uint8_t max) {
    *value = (*value + max - 1) % max;
}

int zmk_custom_config_apply_op(uint8_t op) {
    struct zmk_custom_config next = custom_config;

    switch (op) {
    case CCFG_CPI_UP:
        custom_config_wrap_inc(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case CCFG_CPI_DN:
        custom_config_wrap_dec(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case CCFG_SDIV_UP:
        custom_config_wrap_inc(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case CCFG_SDIV_DN:
        custom_config_wrap_dec(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case CCFG_ROT_UP:
        custom_config_wrap_inc(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case CCFG_ROT_DN:
        custom_config_wrap_dec(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case CCFG_SCALE_TOG:
        next.scaling_mode ^= 1;
        break;
    case CCFG_SCRH_TOG:
        next.scroll_h_rev ^= 1;
        break;
    case CCFG_SCRV_TOG:
        next.scroll_v_rev ^= 1;
        break;
    case CCFG_RESET:
        zmk_custom_config_set_defaults(&next);
        break;
    case CCFG_SAVE:
        zmk_custom_config_log("CCFG_SAVE", &custom_config);
        return custom_feature_save_state();
    default:
        return -ENOTSUP;
    }

    return zmk_custom_config_set_with_tag(&next, custom_config_op_name(op));
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int custom_feature_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                       void *cb_arg) {
    if (!settings_name_steq(name, "state", NULL)) {
        return -ENOENT;
    }

    if (len != sizeof(custom_config)) {
        return -EINVAL;
    }

    int rc = read_cb(cb_arg, &custom_config, sizeof(custom_config));
    if (rc >= 0) {
        settings_init = true;
        zmk_custom_config_changed(&custom_config);
        zmk_custom_config_log("CUSTOM_CFG_LOAD", &custom_config);
        zmk_custom_config_apply_cpi(&custom_config);
        return 0;
    }

    return rc;
}

static int custom_feature_settings_commit(void) {
    if (!settings_init) {
        zmk_custom_config_set_defaults(&custom_config);
        zmk_custom_config_changed(&custom_config);
        zmk_custom_config_log("CUSTOM_CFG_DEFAULTS", &custom_config);
        zmk_custom_config_apply_cpi(&custom_config);
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_feature, "custom_config", NULL,
                               custom_feature_settings_set, custom_feature_settings_commit, NULL);
#endif

static int custom_feature_init(void) {
    return 0;
}

SYS_INIT(custom_feature_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);

