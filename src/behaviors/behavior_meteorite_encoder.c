#define DT_DRV_COMPAT zmk_behavior_meteorite_encoder

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/sensors.h>
#include <zmk/virtual_key_position.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_meteorite_encoder_config {
    const uint16_t *slots;
    size_t slots_len;
    int tap_ms;
};

struct behavior_meteorite_encoder_data {
    struct sensor_value remainder[ZMK_KEYMAP_SENSORS_LEN][ZMK_KEYMAP_LAYERS_LEN];
    int triggers[ZMK_KEYMAP_SENSORS_LEN][ZMK_KEYMAP_LAYERS_LEN];
};

static int met_enc_accept_data(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event,
                               const struct zmk_sensor_config *sensor_config,
                               size_t channel_data_size,
                               const struct zmk_sensor_channel_data *channel_data) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_meteorite_encoder_data *data = dev->data;

    if (channel_data_size < 1) {
        return -EINVAL;
    }

    const struct sensor_value value = channel_data[0].value;
    int triggers;
    int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);

    if (sensor_index < 0 || sensor_index >= ZMK_KEYMAP_SENSORS_LEN) {
        return -EINVAL;
    }

    if (value.val1 == 0) {
        triggers = value.val2;
    } else {
        struct sensor_value remainder = data->remainder[sensor_index][event.layer];

        remainder.val1 += value.val1;
        remainder.val2 += value.val2;

        if (abs(remainder.val2) >= 1000000) {
            remainder.val1 += remainder.val2 / 1000000;
            remainder.val2 %= 1000000;
        }

        int trigger_degrees = 360 / sensor_config->triggers_per_rotation;
        triggers = remainder.val1 / trigger_degrees;
        remainder.val1 %= trigger_degrees;

        data->remainder[sensor_index][event.layer] = remainder;
    }

    data->triggers[sensor_index][event.layer] = triggers;
    return 0;
}

static int met_enc_process(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event,
                           enum behavior_sensor_binding_process_mode mode) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_meteorite_encoder_config *config = dev->config;
    struct behavior_meteorite_encoder_data *data = dev->data;
    int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);

    if (sensor_index < 0 || sensor_index >= ZMK_KEYMAP_SENSORS_LEN) {
        return -EINVAL;
    }

    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        data->triggers[sensor_index][event.layer] = 0;
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    int triggers = data->triggers[sensor_index][event.layer];
    if (triggers == 0) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    size_t direction = 0;
    if (triggers < 0) {
        direction = 1;
        triggers = -triggers;
    }

    size_t slot_offset = (sensor_index * 2) + direction;
    if (slot_offset >= config->slots_len) {
        LOG_WRN("No Meteorite encoder slot for sensor %d direction %u", sensor_index, direction);
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    uint16_t slot = config->slots[slot_offset];
    const struct zmk_behavior_binding *slot_binding =
        zmk_keymap_get_layer_binding_at_idx(event.layer, slot);
    if (slot_binding == NULL || slot_binding->behavior_dev == NULL) {
        LOG_WRN("No Meteorite encoder binding at layer %d slot %u", event.layer, slot);
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

#if IS_ENABLED(CONFIG_ZMK_SPLIT)
    event.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL;
#endif

    for (int i = 0; i < triggers; i++) {
        int ret = zmk_behavior_queue_add(&event, *slot_binding, true, config->tap_ms);
        if (ret < 0) {
            return ret;
        }
        ret = zmk_behavior_queue_add(&event, *slot_binding, false, 0);
        if (ret < 0) {
            return ret;
        }
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_meteorite_encoder_driver_api = {
    .sensor_binding_accept_data = met_enc_accept_data,
    .sensor_binding_process = met_enc_process,
};

#define METEORITE_ENCODER_INST(n)                                                                  \
    static const uint16_t behavior_meteorite_encoder_slots_##n[] = DT_INST_PROP(n, slots);         \
    static const struct behavior_meteorite_encoder_config                                          \
        behavior_meteorite_encoder_config_##n = {                                                  \
            .slots = behavior_meteorite_encoder_slots_##n,                                         \
            .slots_len = ARRAY_SIZE(behavior_meteorite_encoder_slots_##n),                         \
            .tap_ms = DT_INST_PROP(n, tap_ms),                                                     \
        };                                                                                         \
    static struct behavior_meteorite_encoder_data behavior_meteorite_encoder_data_##n = {};        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_meteorite_encoder_data_##n,                   \
                            &behavior_meteorite_encoder_config_##n, POST_KERNEL,                   \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_meteorite_encoder_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_ENCODER_INST)
