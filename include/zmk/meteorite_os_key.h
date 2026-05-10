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

#define METEORITE_OS_KEY_SEP_NONE()
#define METEORITE_OS_KEY_SEP_COMMA() ,

#define METEORITE_OS_KEY_LIST_TAIL(X, ARG, SEP)                                                    \
    X(ARG, M_OS_ALT_OPT, "Alt/Opt", LALT, LALT) SEP()                                             \
    X(ARG, M_OS_ALT_CTRL, "Alt/Ctrl", LALT, LCTRL) SEP()                                          \
    X(ARG, M_OS_WIN_CTRL, "Win/Ctrl", LGUI, LCTRL) SEP()                                          \
    X(ARG, M_OS_WIN_OPT, "Win/Opt", LGUI, LALT) SEP()                                             \
    X(ARG, M_OS_ALT_CMD, "Alt/Cmd", LALT, LGUI)

#define METEORITE_OS_KEY_LIST(X, ARG, SEP)                                                        \
    X(ARG, M_OS_CTRL_CMD, "Ctrl/Cmd", LCTRL, LGUI) SEP()                                          \
    METEORITE_OS_KEY_LIST_TAIL(X, ARG, SEP)

#define METEORITE_OS_KEY_LIST_EXCLUDING_CTRL_CMD(X, ARG, SEP)                                      \
    METEORITE_OS_KEY_LIST_TAIL(X, ARG, SEP)

static inline bool meteorite_os_keycode_for_param(uint32_t param, bool is_mac, uint32_t *keycode) {
    if (param == 0) {
        param = M_OS_CTRL_CMD;
    }

#define METEORITE_OS_KEYCODE_CASE(_arg, id, display, win_key, mac_key)                            \
    case id:                                                                                      \
        *keycode = is_mac ? (mac_key) : (win_key);                                                 \
        return true;

    switch (param) {
        METEORITE_OS_KEY_LIST(METEORITE_OS_KEYCODE_CASE, unused, METEORITE_OS_KEY_SEP_NONE)
    default:
        return false;
    }

#undef METEORITE_OS_KEYCODE_CASE
}

#define METEORITE_OS_KEY_METADATA_VALUE(VAL_MACRO, id, display, win_key, mac_key)                 \
    {                                                                                             \
        .display_name = "OS-Switch Mod " display,                                                 \
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                              \
        .value = VAL_MACRO(id),                                                                   \
    }

#define METEORITE_OS_KEY_METADATA_VALUES(VAL_MACRO)                                               \
    METEORITE_OS_KEY_LIST(METEORITE_OS_KEY_METADATA_VALUE, VAL_MACRO,                             \
                          METEORITE_OS_KEY_SEP_COMMA)

#define METEORITE_OS_KEY_METADATA_VALUES_EXCLUDING_CTRL_CMD(VAL_MACRO)                             \
    METEORITE_OS_KEY_LIST_EXCLUDING_CTRL_CMD(METEORITE_OS_KEY_METADATA_VALUE, VAL_MACRO,           \
                                             METEORITE_OS_KEY_SEP_COMMA)

#define METEORITE_OS_KEY_METADATA_VALUES_WITH_DEFAULT(DEFAULT_VAL, VAL_MACRO)                     \
    {                                                                                             \
        .display_name = "Select OS-Switch Mod",                                                   \
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                              \
        .value = (DEFAULT_VAL),                                                                   \
    },                                                                                            \
        METEORITE_OS_KEY_METADATA_VALUES(VAL_MACRO)
