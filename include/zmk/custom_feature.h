#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct zmk_custom_config {
    uint8_t cpi_idx;
    uint8_t scroll_div;
    uint8_t rotation_idx;
    uint8_t scroll_h_rev;
    uint8_t scroll_v_rev;
    uint8_t scaling_mode;
    uint8_t scroll_layer_1;
    uint8_t scroll_layer_2;
};

const struct zmk_custom_config *zmk_custom_config_get(void);
int zmk_custom_config_set(const struct zmk_custom_config *cfg);
int zmk_custom_config_apply_op(uint8_t op);

uint16_t zmk_custom_config_cpi_value(void);
uint16_t zmk_custom_config_scroll_div_value(void);
int16_t zmk_custom_config_rotation_deg(void);
bool zmk_custom_config_scroll_h_rev(void);
bool zmk_custom_config_scroll_v_rev(void);
bool zmk_custom_config_scaling_enabled(void);
uint8_t zmk_custom_config_scroll_layer_1(void);
uint8_t zmk_custom_config_scroll_layer_2(void);

/* Optional hook to react to state changes from settings or toggles. */
void zmk_custom_config_changed(const struct zmk_custom_config *cfg);

#ifdef __cplusplus
}
#endif
