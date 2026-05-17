/*
 * Copyright (c) 2026 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 * Adapter: expose zmk_custom_config values to prospector-zmk-module's
 * v2 Extended Advertising builder.
 *
 * Bridges two otherwise independent modules:
 *   - zmk-feature-meteorite-config: owns the runtime custom config struct
 *     (OS mode, CPI, scroll layer indices) and the change-notification hook.
 *   - prospector-zmk-module: builds & broadcasts the v2 ADV packet, but
 *     uses weak getters so it stays buildable without this feature.
 *
 * This file overrides those weak getters with real implementations and
 * hooks zmk_custom_config_changed() to trigger an immediate v2 re-broadcast
 * so the new values appear within one ADV cycle of a Studio-side change.
 *
 * Compiled only when both CONFIG_ZMK_CUSTOM_CONFIG and
 * CONFIG_PROSPECTOR_STATUS_ADV_V2_EXT are enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/custom_feature.h>
#include <zmk/prospector_v2_hooks.h>
#include <zmk/status_advertisement_v2.h>

LOG_MODULE_REGISTER(prospector_v2_adapter, CONFIG_ZMK_LOG_LEVEL);

/* ============================== Getters ============================ */

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

/* ============================== Change hook ======================== */

/* Overrides the __weak default in state.c. Called on every mutation of
 * the custom config (set / save / discard / reset). */
void zmk_custom_config_changed(const struct zmk_custom_config *cfg) {
    ARG_UNUSED(cfg);
    zmk_status_advertisement_v2_notify_changed();
}
