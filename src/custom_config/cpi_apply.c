/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define TRACKBALL_NODE DT_NODELABEL(trackball)

#ifndef PMW3610_ATTR_CPI
/* Keep in sync with zmk-pmw3610-driver/src/pmw3610.h */
#define PMW3610_ATTR_CPI 0
#endif

void zmk_custom_config_apply_cpi(const struct zmk_custom_config *cfg) {
#if DT_NODE_EXISTS(TRACKBALL_NODE)
    const struct device *dev = DEVICE_DT_GET(TRACKBALL_NODE);
    uint16_t cpi = zmk_custom_config_cpi_value_for(cfg);

    if (!device_is_ready(dev)) {
        LOG_WRN("CPI apply skipped: trackball device not ready (cpi=%u)", cpi);
        return;
    }

    struct sensor_value val = {
        .val1 = cpi,
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
