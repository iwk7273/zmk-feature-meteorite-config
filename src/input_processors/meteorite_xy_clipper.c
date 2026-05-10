#define DT_DRV_COMPAT zmk_input_processor_meteorite_xy_clipper

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
#include <zmk/custom_feature.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct meteorite_xy_clipper_data {
    int32_t x;
    int32_t y;
    int32_t effective_threshold;
    int32_t last_threshold;
    int8_t last_invert_x;
    int8_t last_invert_y;
};

struct meteorite_xy_clipper_config {
    int32_t threshold;
    int invert_x;
    int invert_y;
};

static int32_t
meteorite_xy_clipper_get_threshold(const struct device *dev,
                                   struct meteorite_xy_clipper_data *data,
                                   const struct meteorite_xy_clipper_config *config) {
    int32_t threshold = config->threshold;

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    threshold = (int32_t)zmk_custom_config_scroll_div_value();
    data->effective_threshold = 0;
#endif

    if (data->effective_threshold > 0) {
        return data->effective_threshold;
    }

    if (threshold <= 0) {
        LOG_WRN("%s: invalid threshold %d, clamping to 1", dev->name, threshold);
        threshold = 1;
    }

    data->effective_threshold = threshold;
    return data->effective_threshold;
}

static int meteorite_xy_clipper_handle_event(const struct device *dev, struct input_event *event,
                                             uint32_t param1, uint32_t param2,
                                             struct zmk_input_processor_state *state) {
    struct meteorite_xy_clipper_data *data = dev->data;
    const struct meteorite_xy_clipper_config *config = dev->config;
    int32_t threshold = meteorite_xy_clipper_get_threshold(dev, data, config);
    bool invert_x = config->invert_x != 0;
    bool invert_y = config->invert_y != 0;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    invert_x = zmk_custom_config_scroll_h_rev();
    invert_y = zmk_custom_config_scroll_v_rev();
#endif

    if (threshold != data->last_threshold || invert_x != data->last_invert_x ||
        invert_y != data->last_invert_y) {
        LOG_INF("meteorite_xy_clipper cfg threshold=%d invert_x=%d invert_y=%d", threshold,
                invert_x, invert_y);
        data->last_threshold = threshold;
        data->last_invert_x = invert_x;
        data->last_invert_y = invert_y;
    }

    switch (event->type) {
    case INPUT_EV_REL:
        if (event->code == INPUT_REL_X) {
            data->x += event->value;
            event->value = 0;
        } else if (event->code == INPUT_REL_Y) {
            data->y += event->value;
            event->value = 0;
        } else {
            return ZMK_INPUT_PROC_CONTINUE;
        }

        bool x_triggered = abs(data->x) >= threshold;
        bool y_triggered = abs(data->y) >= threshold;
        LOG_DBG("meteorite_xy_clipper acc x=%d y=%d thr=%d x_trig=%d y_trig=%d sync=%d",
                data->x, data->y, threshold, x_triggered, y_triggered, event->sync);

        if (y_triggered && (!x_triggered || (abs(data->y) * 2) >= abs(data->x))) {
            event->code = INPUT_REL_Y;
            int32_t val = data->y / threshold;
            event->value = invert_y ? -val : val;
            data->y %= threshold;
            data->x = 0;
            LOG_DBG("meteorite_xy_clipper choose Y val=%d rem_y=%d reset_x=0 sync=%d",
                    event->value, data->y, event->sync);
            return ZMK_INPUT_PROC_CONTINUE;
        } else if (x_triggered) {
            event->code = INPUT_REL_X;
            int32_t val = data->x / threshold;
            event->value = invert_x ? -val : val;
            data->x %= threshold;
            data->y = 0;
            LOG_DBG("meteorite_xy_clipper choose X val=%d rem_x=%d reset_y=0 sync=%d",
                    event->value, data->x, event->sync);
            return ZMK_INPUT_PROC_CONTINUE;
        }

        LOG_DBG("meteorite_xy_clipper stop (no trigger)");
        return ZMK_INPUT_PROC_CONTINUE;

    default:
        return ZMK_INPUT_PROC_CONTINUE;
    }
}

static struct zmk_input_processor_driver_api meteorite_xy_clipper_driver_api = {
    .handle_event = meteorite_xy_clipper_handle_event,
};

#define METEORITE_XY_CLIPPER_INST(n)                                                              \
    static struct meteorite_xy_clipper_data meteorite_xy_clipper_data_##n = {                      \
        .x = 0,                                                                                    \
        .y = 0,                                                                                    \
        .effective_threshold = 0,                                                                  \
        .last_threshold = INT32_MIN,                                                               \
        .last_invert_x = -1,                                                                       \
        .last_invert_y = -1,                                                                       \
    };                                                                                             \
    static const struct meteorite_xy_clipper_config meteorite_xy_clipper_config_##n = {            \
        .threshold = DT_INST_PROP_OR(n, threshold, 1),                                             \
        .invert_x = DT_INST_PROP_OR(n, invert_x, 0),                                               \
        .invert_y = DT_INST_PROP_OR(n, invert_y, 0),                                               \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_xy_clipper_data_##n,                           \
                          &meteorite_xy_clipper_config_##n, POST_KERNEL,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &meteorite_xy_clipper_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_XY_CLIPPER_INST)
