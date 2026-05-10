#pragma once

#include <stdint.h>

#define ZMK_CUSTOM_CONFIG_STRINGIFY_VALUE(value) #value
#define ZMK_CUSTOM_CONFIG_STRINGIFY(value) ZMK_CUSTOM_CONFIG_STRINGIFY_VALUE(value)

#define ZMK_CUSTOM_CONFIG_CPI_STEP 200
#define ZMK_CUSTOM_CONFIG_CPI_STEP_STR ZMK_CUSTOM_CONFIG_STRINGIFY(ZMK_CUSTOM_CONFIG_CPI_STEP)
#define ZMK_CUSTOM_CONFIG_CPI_MAX 16

#define ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP 5
#define ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP_STR                                                     \
    ZMK_CUSTOM_CONFIG_STRINGIFY(ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP)
#define ZMK_CUSTOM_CONFIG_SCROLL_DIV_MAX 16

struct zmk_custom_config_axis {
    uint16_t step;
    uint8_t max;
};

static inline struct zmk_custom_config_axis zmk_custom_config_cpi_axis(void) {
    struct zmk_custom_config_axis axis = {
        ZMK_CUSTOM_CONFIG_CPI_STEP,
        ZMK_CUSTOM_CONFIG_CPI_MAX,
    };

    return axis;
}

static inline struct zmk_custom_config_axis zmk_custom_config_scroll_div_axis(void) {
    struct zmk_custom_config_axis axis = {
        ZMK_CUSTOM_CONFIG_SCROLL_DIV_STEP,
        ZMK_CUSTOM_CONFIG_SCROLL_DIV_MAX,
    };

    return axis;
}

static inline uint16_t zmk_custom_config_axis_idx_to_value(struct zmk_custom_config_axis axis,
                                                           uint8_t index) {
    return (uint16_t)(((uint16_t)index + 1U) * axis.step);
}

static inline uint8_t zmk_custom_config_axis_value_to_idx(struct zmk_custom_config_axis axis,
                                                          int32_t value) {
    if (axis.step == 0U || axis.max == 0U) {
        return 0;
    }

    int32_t index = ((value + ((int32_t)axis.step / 2)) / (int32_t)axis.step) - 1;
    if (index < 0) {
        return 0;
    }
    if (index >= axis.max) {
        return (uint8_t)(axis.max - 1U);
    }

    return (uint8_t)index;
}
