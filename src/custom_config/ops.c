/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>

#include <dt-bindings/zmk/custom_config.h>

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

static void custom_config_wrap_inc(uint8_t *value, uint8_t max) {
    *value = (*value + 1) % max;
}

static void custom_config_wrap_dec(uint8_t *value, uint8_t max) {
    *value = (*value + max - 1) % max;
}

int zmk_custom_config_apply_op(uint8_t op) {
    struct zmk_custom_config next = *zmk_custom_config_get();

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
        custom_config_wrap_inc(&next.rotation_idx, CUSTOM_ROTATION_ANGLE_COUNT);
        break;
    case C_ROT_DN:
        custom_config_wrap_dec(&next.rotation_idx, CUSTOM_ROTATION_ANGLE_COUNT);
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
        zmk_custom_config_log("C_SAVE", zmk_custom_config_get());
        return zmk_custom_config_save();
    default:
        return -ENOTSUP;
    }

    zmk_custom_config_sanitize_layers(&next);
    return zmk_custom_config_set_with_tag(&next, custom_config_op_name(op));
}
