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

#include <dt-bindings/zmk/custom_config.h>
#include <zmk/custom_feature.h>

#define CUSTOM_CPI_DEFAULT 5
#define CUSTOM_CPI_MAX 32
#define CUSTOM_SCROLL_DIV_DEFAULT 5
#define CUSTOM_SCROLL_DIV_MAX 16
#define CUSTOM_ROTATION_DEFAULT 5

static const int16_t rotation_angles[] = {-70, -60, -50, -40, -30, -20, -10, 0,
                                          10,  20,  30,  40,  50,  60,  70};
#define ROTATION_ANGLE_COUNT (sizeof(rotation_angles) / sizeof(rotation_angles[0]))

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_custom_config custom_config;

#if IS_ENABLED(CONFIG_SETTINGS)
static bool settings_init;
static struct k_work_delayable save_work;

static void custom_feature_save_state_work(struct k_work *work) {
    ARG_UNUSED(work);
    settings_save_one("custom_config/state", &custom_config, sizeof(custom_config));
}

static int custom_feature_save_state(void) {
    int ret = k_work_reschedule(&save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(ret, 0);
}
#else
static inline int custom_feature_save_state(void) { return 0; }
#endif

__weak void zmk_custom_config_changed(const struct zmk_custom_config *cfg) { ARG_UNUSED(cfg); }

static void zmk_custom_config_set_defaults(struct zmk_custom_config *cfg) {
    cfg->cpi_idx = CUSTOM_CPI_DEFAULT;
    cfg->scroll_div = CUSTOM_SCROLL_DIV_DEFAULT;
    cfg->rotation_idx = CUSTOM_ROTATION_DEFAULT;
    cfg->scroll_h_rev = 0;
    cfg->scroll_v_rev = 0;
    cfg->scaling_mode = 0;
}

static bool zmk_custom_config_equals(const struct zmk_custom_config *a,
                                     const struct zmk_custom_config *b) {
    return memcmp(a, b, sizeof(*a)) == 0;
}

const struct zmk_custom_config *zmk_custom_config_get(void) { return &custom_config; }

int zmk_custom_config_set(const struct zmk_custom_config *cfg) {
    if (zmk_custom_config_equals(&custom_config, cfg)) {
        return 0;
    }

    custom_config = *cfg;
    zmk_custom_config_changed(&custom_config);
    return custom_feature_save_state();
}

uint16_t zmk_custom_config_cpi_value(void) { return (custom_config.cpi_idx + 1) * 100; }

uint16_t zmk_custom_config_scroll_div_value(void) {
    return (custom_config.scroll_div * custom_config.scroll_div * 2) + 10;
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
    case CUSTOM_CFG_CPI_INC:
        custom_config_wrap_inc(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case CUSTOM_CFG_CPI_DEC:
        custom_config_wrap_dec(&next.cpi_idx, CUSTOM_CPI_MAX);
        break;
    case CUSTOM_CFG_SCROLL_DIV_INC:
        custom_config_wrap_inc(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case CUSTOM_CFG_SCROLL_DIV_DEC:
        custom_config_wrap_dec(&next.scroll_div, CUSTOM_SCROLL_DIV_MAX);
        break;
    case CUSTOM_CFG_ROT_INC:
        custom_config_wrap_inc(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case CUSTOM_CFG_ROT_DEC:
        custom_config_wrap_dec(&next.rotation_idx, ROTATION_ANGLE_COUNT);
        break;
    case CUSTOM_CFG_SCALING_TOGGLE:
        next.scaling_mode ^= 1;
        break;
    case CUSTOM_CFG_SCROLL_H_REV_TOGGLE:
        next.scroll_h_rev ^= 1;
        break;
    case CUSTOM_CFG_SCROLL_V_REV_TOGGLE:
        next.scroll_v_rev ^= 1;
        break;
    case CUSTOM_CFG_RESET_DEFAULTS:
        zmk_custom_config_set_defaults(&next);
        break;
    case CUSTOM_CFG_SAVE:
        return custom_feature_save_state();
    default:
        return -ENOTSUP;
    }

    return zmk_custom_config_set(&next);
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
        return 0;
    }

    return rc;
}

static int custom_feature_settings_commit(void) {
    if (!settings_init) {
        zmk_custom_config_set_defaults(&custom_config);
        zmk_custom_config_changed(&custom_config);
        k_work_schedule(&save_work, K_NO_WAIT);
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_feature, "custom_config", NULL,
                               custom_feature_settings_set, custom_feature_settings_commit, NULL);
#endif

static int custom_feature_init(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_init_delayable(&save_work, custom_feature_save_state_work);
#endif
    return 0;
}

SYS_INIT(custom_feature_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
