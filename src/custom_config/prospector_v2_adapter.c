/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Adapter: expose zmk_custom_config values to prospector-zmk-module's
 * v2 Extended Advertising builder.
 *
 * prospector-zmk-module declares five __weak getters for the keyboard-
 * specific metadata it can't know about (OS mode, CPI, scroll layer
 * indices). This file overrides them by reading from the zmk_custom_config
 * state owned by zmk-feature-meteorite-config.
 *
 * No change-notification hook is wired up: zmk_custom_config_changed()
 * is already implemented (non-weak) in the ZMK fork's Studio subsystem,
 * so adding another definition here would multiply-define the symbol.
 * The v2 ADV refresh tick runs at ~1Hz, so config edits show up on the
 * scanner within one cycle — fine for a status display.
 *
 * Compiled only when both CONFIG_ZMK_CUSTOM_CONFIG and
 * CONFIG_PROSPECTOR_STATUS_ADV_V2_EXT are enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/custom_feature.h>
#include <zmk/prospector_v2_hooks.h>

LOG_MODULE_REGISTER(prospector_v2_adapter, CONFIG_ZMK_LOG_LEVEL);

uint8_t prospector_v2_get_os_mode(void) {
    return zmk_custom_config_get()->os_mode;
}

uint16_t prospector_v2_get_cpi(void) {
    return zmk_custom_config_cpi_value();
}

uint8_t prospector_v2_get_scroll_layer_1(void) {
    return zmk_custom_config_scroll_layer_1();
}

uint8_t prospector_v2_get_scroll_layer_2(void) {
    return zmk_custom_config_scroll_layer_2();
}

uint16_t prospector_v2_get_scroll_div(void) {
    return zmk_custom_config_scroll_div_value();
}
