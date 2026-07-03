#define DT_DRV_COMPAT zmk_input_processor_meteorite_sensor_rotation

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <limits.h>
#include <stdint.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
#include <zmk/custom_feature.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define ROTATION_FP_SCALE  1000 /* sin/cos lookup tables are scaled by this factor */
#define ROTATION_STEP_DEG  5    /* angle resolution of the lookup tables / snap step */
#define ROTATION_CLAMP_DEG 70   /* rotation angle is clamped to +/- this many degrees */

struct meteorite_sensor_rotation_config {
    int rotation_angle;
};

struct meteorite_sensor_rotation_data {
    int32_t x;
    int32_t y;
    int16_t sin_val;
    int16_t cos_val;
    int rotation_angle;
};

static const int16_t sin_table[] = {
    0,   87,  174, 259, 342, 423, 500, 574, 643, 707,
    766, 819, 866, 906, 940, 966, 985, 996, 1000,
};

static const int16_t cos_table[] = {
    1000, 996, 985, 966, 940, 906, 866, 819, 766, 707,
    643,  574, 500, 423, 342, 259, 174, 87,  0,
};

static void lookup_sin_cos(int angle, int16_t *sin_val, int16_t *cos_val) {
    angle = angle % 360;
    if (angle < 0) {
        angle += 360;
    }

    int index = (angle % 90) / ROTATION_STEP_DEG;
    int quadrant = angle / 90;
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
    }
}

static int clamp_rotation_angle(int angle) {
    if (angle < -ROTATION_CLAMP_DEG) {
        return -ROTATION_CLAMP_DEG;
    }

    if (angle > ROTATION_CLAMP_DEG) {
        return ROTATION_CLAMP_DEG;
    }

    return angle;
}

static int snap_rotation_angle(int angle) {
    if (angle >= 0) {
        return (angle / ROTATION_STEP_DEG) * ROTATION_STEP_DEG;
    }

    return -(((-angle) / ROTATION_STEP_DEG) * ROTATION_STEP_DEG);
}

static void update_rotation_angle(struct meteorite_sensor_rotation_data *data, int angle) {
    angle = snap_rotation_angle(clamp_rotation_angle(angle));
    if (angle == data->rotation_angle) {
        return;
    }

    data->rotation_angle = angle;
    lookup_sin_cos(angle, &data->sin_val, &data->cos_val);
}

static void rotate_xy(int32_t *x, int32_t *y, int16_t sin_val, int16_t cos_val) {
    int32_t new_x = (*x * cos_val - *y * sin_val) / ROTATION_FP_SCALE;
    int32_t new_y = (*x * sin_val + *y * cos_val) / ROTATION_FP_SCALE;

    *x = new_x;
    *y = new_y;
}

static int meteorite_sensor_rotation_handle_event(const struct device *dev,
                                                  struct input_event *event, uint32_t param1,
                                                  uint32_t param2,
                                                  struct zmk_input_processor_state *state) {
    struct meteorite_sensor_rotation_data *data = dev->data;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    update_rotation_angle(data, zmk_custom_config_rotation_deg());
#endif

    int32_t temp_x = (event->code == INPUT_REL_X) ? event->value : data->x;
    int32_t temp_y = (event->code == INPUT_REL_Y) ? event->value : data->y;
    rotate_xy(&temp_x, &temp_y, data->sin_val, data->cos_val);

    if (event->code == INPUT_REL_X) {
        data->x = event->value;
        event->value = temp_x;
        LOG_DBG("meteorite_sensor_rotation X value=%d rotate=%d x=%d y=%d sin=%d cos=%d",
                event->value, data->rotation_angle, data->x, data->y, data->sin_val,
                data->cos_val);
    } else { /* INPUT_REL_Y, guaranteed by the guard above */
        data->y = event->value;
        event->value = temp_y;
        LOG_DBG("meteorite_sensor_rotation Y value=%d rotate=%d x=%d y=%d sin=%d cos=%d",
                event->value, data->rotation_angle, data->x, data->y, data->sin_val,
                data->cos_val);
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static int meteorite_sensor_rotation_init(const struct device *dev) {
    struct meteorite_sensor_rotation_data *data = dev->data;

    data->x = 0;
    data->y = 0;
    data->rotation_angle = INT_MAX;

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    update_rotation_angle(data, zmk_custom_config_rotation_deg());
#else
    const struct meteorite_sensor_rotation_config *cfg = dev->config;
    update_rotation_angle(data, cfg->rotation_angle);
#endif

    return 0;
}

static struct zmk_input_processor_driver_api meteorite_sensor_rotation_driver_api = {
    .handle_event = meteorite_sensor_rotation_handle_event,
};

#define METEORITE_SENSOR_ROTATION_INST(n)                                                         \
    static struct meteorite_sensor_rotation_data meteorite_sensor_rotation_data_##n = {};          \
    static const struct meteorite_sensor_rotation_config meteorite_sensor_rotation_config_##n = {  \
        .rotation_angle = DT_INST_PROP(n, rotation_angle),                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, meteorite_sensor_rotation_init, NULL,                                 \
                          &meteorite_sensor_rotation_data_##n,                                     \
                          &meteorite_sensor_rotation_config_##n, POST_KERNEL,                      \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &meteorite_sensor_rotation_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_SENSOR_ROTATION_INST)
