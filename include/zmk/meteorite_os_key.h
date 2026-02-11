/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <drivers/behavior.h>

#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/meteorite_custom_keys.h>

static inline bool meteorite_os_keycode_for_param(uint32_t param, bool is_mac, uint32_t *keycode) {
    if (param == 0) {
        param = M_OS_CTRL_CMD;
    }

    switch (param) {
    case M_OS_CTRL_CMD:
        *keycode = is_mac ? LGUI : LCTRL;
        return true;
    case M_OS_ALT_OPT:
        *keycode = LALT;
        return true;
    case M_OS_ALT_CTRL:
        *keycode = is_mac ? LCTRL : LALT;
        return true;
    case M_OS_WIN_CTRL:
        *keycode = is_mac ? LCTRL : LGUI;
        return true;
    case M_OS_WIN_OPT:
        *keycode = is_mac ? LALT : LGUI;
        return true;
    case M_OS_ALT_CMD:
        *keycode = is_mac ? LGUI : LALT;
        return true;
    default:
        return false;
    }
}

#define METEORITE_OS_KEY_METADATA_VALUES(VAL_MACRO)                                               \
    {                                                                                             \
        .display_name = "OS Ctrl/Cmd",                                                            \
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                              \
        .value = VAL_MACRO(M_OS_CTRL_CMD),                                                        \
    },                                                                                            \
        {                                                                                         \
            .display_name = "OS Alt/Opt",                                                         \
            .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                          \
            .value = VAL_MACRO(M_OS_ALT_OPT),                                                     \
        },                                                                                        \
        {                                                                                         \
            .display_name = "OS Alt/Ctrl",                                                        \
            .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                          \
            .value = VAL_MACRO(M_OS_ALT_CTRL),                                                    \
        },                                                                                        \
        {                                                                                         \
            .display_name = "OS Win/Ctrl",                                                        \
            .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                          \
            .value = VAL_MACRO(M_OS_WIN_CTRL),                                                    \
        },                                                                                        \
        {                                                                                         \
            .display_name = "OS Win/Opt",                                                         \
            .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                          \
            .value = VAL_MACRO(M_OS_WIN_OPT),                                                     \
        },                                                                                        \
        {                                                                                         \
            .display_name = "OS Alt/Cmd",                                                         \
            .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                          \
            .value = VAL_MACRO(M_OS_ALT_CMD),                                                     \
        }

#define METEORITE_OS_KEY_METADATA_VALUES_WITH_DEFAULT(DEFAULT_VAL, VAL_MACRO)                     \
    {                                                                                             \
        .display_name = "Default (OS Ctrl/Cmd)",                                                   \
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                              \
        .value = (DEFAULT_VAL),                                                                   \
    },                                                                                            \
        METEORITE_OS_KEY_METADATA_VALUES(VAL_MACRO)
