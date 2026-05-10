/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

/*
 * These must remain preprocessor defines because keymap/DTS files include this header.
 * The C dispatch and Studio metadata table lives in <zmk/custom_config_ops.def> and asserts
 * these values stay in sync.
 */
#define C_CPI_UP 1
#define C_CPI_DN 2
#define C_SDIV_UP 3
#define C_SDIV_DN 4
#define C_ROT_UP 5
#define C_ROT_DN 6
#define C_SCALE_TOG 7
#define C_SCRH_TOG 8
#define C_SCRV_TOG 9
#define C_SCRL1_UP 10
#define C_SCRL2_UP 11
#define C_RESET 12
#define C_SAVE 13
#define C_SCRL_SCALE_TOG 14
#define C_OS_TOG 15
#define C_OS_WIN 16
#define C_OS_MAC 17
