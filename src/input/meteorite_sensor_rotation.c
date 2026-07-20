#define DT_DRV_COMPAT zmk_input_meteorite_sensor_rotation

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
#include <zmk/custom_feature.h>
#endif

LOG_MODULE_REGISTER(meteorite_sensor_rotation, CONFIG_ZMK_LOG_LEVEL);

#define ROTATION_FP_SCALE 1000
#define ROTATION_STEP_DEG 5
#define ROTATION_CLAMP_DEG 70

struct meteorite_sensor_rotation_config {
    int32_t rotation_angle;
};

struct meteorite_sensor_rotation_data {
    int32_t frame_x;
    int32_t frame_y;
    int32_t remainder_x;
    int32_t remainder_y;
    int32_t rotation_angle;
    bool frame_pending;
};

static const int16_t sin_table[] = {
    0,   87,  174, 259, 342, 423, 500, 574, 643, 707,
    766, 819, 866, 906, 940, 966, 985, 996, 1000,
};

static const int16_t cos_table[] = {
    1000, 996, 985, 966, 940, 906, 866, 819, 766, 707,
    643,  574, 500, 423, 342, 259, 174, 87,  0,
};

static int32_t saturating_add_i32(int32_t left, int32_t right) {
    if (right > 0 && left > INT32_MAX - right) {
        return INT32_MAX;
    }
    if (right < 0 && left < INT32_MIN - right) {
        return INT32_MIN;
    }
    return left + right;
}

static int32_t sanitize_rotation_angle(int32_t angle) {
    angle = CLAMP(angle, -ROTATION_CLAMP_DEG, ROTATION_CLAMP_DEG);
    if (angle >= 0) {
        return ((angle + ROTATION_STEP_DEG / 2) / ROTATION_STEP_DEG) * ROTATION_STEP_DEG;
    }
    return -(((-angle + ROTATION_STEP_DEG / 2) / ROTATION_STEP_DEG) * ROTATION_STEP_DEG);
}

static void lookup_sin_cos(int32_t angle, int16_t *sin_val, int16_t *cos_val) {
    int32_t normalized = angle % 360;
    if (normalized < 0) {
        normalized += 360;
    }

    int32_t index = (normalized % 90) / ROTATION_STEP_DEG;
    int32_t quadrant = normalized / 90;
    int16_t sin_base = sin_table[index];
    int16_t cos_base = cos_table[index];

    switch (quadrant) {
    case 0:
        *sin_val = sin_base;
        *cos_val = cos_base;
        break;
    case 1:
        *sin_val = cos_base;
        *cos_val = -sin_base;
        break;
    case 2:
        *sin_val = -sin_base;
        *cos_val = -cos_base;
        break;
    case 3:
        *sin_val = -cos_base;
        *cos_val = sin_base;
        break;
    default:
        CODE_UNREACHABLE;
    }
}

static uint64_t magnitude_i64(int64_t value) {
    return value < 0 ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static int64_t divide_round_nearest(int64_t value) {
    uint64_t magnitude = magnitude_i64(value);
    int64_t rounded = (int64_t)((magnitude + ROTATION_FP_SCALE / 2) / ROTATION_FP_SCALE);
    return value < 0 ? -rounded : rounded;
}

static int32_t quantize_axis(int64_t numerator, int32_t *remainder) {
    int64_t accumulated = numerator + *remainder;
    int64_t output = divide_round_nearest(accumulated);

    if (output > INT32_MAX) {
        *remainder = 0;
        return INT32_MAX;
    }
    if (output < INT32_MIN) {
        *remainder = 0;
        return INT32_MIN;
    }

    *remainder = (int32_t)(accumulated - output * ROTATION_FP_SCALE);
    return (int32_t)output;
}

static int32_t current_rotation_angle(const struct meteorite_sensor_rotation_config *config) {
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    ARG_UNUSED(config);
    return sanitize_rotation_angle(zmk_custom_config_rotation_deg());
#else
    return sanitize_rotation_angle(config->rotation_angle);
#endif
}

static int report_proxy_event(const struct device *dev, uint8_t type, uint16_t code,
                              int32_t value, bool sync) {
    int err = input_report(dev, type, code, value, sync, K_NO_WAIT);
    if (err < 0) {
        LOG_ERR("Failed to report rotated event type=%u code=%u sync=%u (%d)",
                (unsigned int)type, (unsigned int)code, (unsigned int)sync, err);
    }
    return err;
}

static void flush_rotated_frame(const struct device *dev, bool sync) {
    const struct meteorite_sensor_rotation_config *config = dev->config;
    struct meteorite_sensor_rotation_data *data = dev->data;

    int32_t frame_x = data->frame_x;
    int32_t frame_y = data->frame_y;
    data->frame_x = 0;
    data->frame_y = 0;
    data->frame_pending = false;

    int32_t angle = current_rotation_angle(config);
    if (angle != data->rotation_angle) {
        data->rotation_angle = angle;
        data->remainder_x = 0;
        data->remainder_y = 0;
    }

    int16_t sin_val;
    int16_t cos_val;
    lookup_sin_cos(angle, &sin_val, &cos_val);

    int64_t rotated_x = (int64_t)frame_x * cos_val - (int64_t)frame_y * sin_val;
    int64_t rotated_y = (int64_t)frame_x * sin_val + (int64_t)frame_y * cos_val;
    int32_t output_x = quantize_axis(rotated_x, &data->remainder_x);
    int32_t output_y = quantize_axis(rotated_y, &data->remainder_y);

    LOG_DBG("rotation angle=%d raw=(%d,%d) out=(%d,%d) rem=(%d,%d)", angle, frame_x,
            frame_y, output_x, output_y, data->remainder_x, data->remainder_y);

    int x_err = report_proxy_event(dev, INPUT_EV_REL, INPUT_REL_X, output_x, false);
    int y_err = report_proxy_event(dev, INPUT_EV_REL, INPUT_REL_Y, output_y, sync);
    if (x_err < 0 || y_err < 0) {
        /* The frame was not delivered completely. Do not let its fractional
         * state alter a later, otherwise independent frame. */
        data->remainder_x = 0;
        data->remainder_y = 0;
    }
}

static void meteorite_sensor_rotation_input_handler(const struct device *dev,
                                                    struct input_event *event) {
    struct meteorite_sensor_rotation_data *data = dev->data;
    bool is_relative_axis = event->type == INPUT_EV_REL &&
                            (event->code == INPUT_REL_X || event->code == INPUT_REL_Y);

    if (is_relative_axis) {
        if (event->code == INPUT_REL_X) {
            data->frame_x = saturating_add_i32(data->frame_x, event->value);
        } else {
            data->frame_y = saturating_add_i32(data->frame_y, event->value);
        }
        data->frame_pending = true;

        if (event->sync) {
            flush_rotated_frame(dev, true);
        }
        return;
    }

    if (event->sync && data->frame_pending) {
        flush_rotated_frame(dev, false);
    }
    report_proxy_event(dev, event->type, event->code, event->value, event->sync);
}

#define METEORITE_SENSOR_ROTATION_INST(n)                                                         \
    static struct meteorite_sensor_rotation_data meteorite_sensor_rotation_data_##n = {           \
        .rotation_angle = INT32_MAX,                                                               \
    };                                                                                             \
    static const struct meteorite_sensor_rotation_config meteorite_sensor_rotation_config_##n = { \
        .rotation_angle = DT_INST_PROP(n, rotation_angle),                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_sensor_rotation_data_##n,                      \
                          &meteorite_sensor_rotation_config_##n, POST_KERNEL,                      \
                          UTIL_INC(CONFIG_INPUT_INIT_PRIORITY), NULL);                             \
    static void meteorite_sensor_rotation_input_handler_##n(struct input_event *event,             \
                                                            void *user_data) {                     \
        ARG_UNUSED(user_data);                                                                     \
        meteorite_sensor_rotation_input_handler(DEVICE_DT_INST_GET(n), event);                     \
    }                                                                                              \
    INPUT_CALLBACK_DEFINE_NAMED(DEVICE_DT_GET(DT_INST_PHANDLE(n, device)),                         \
                                meteorite_sensor_rotation_input_handler_##n, NULL,                 \
                                meteorite_sensor_rotation_##n);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_SENSOR_ROTATION_INST)
