#define DT_DRV_COMPAT zmk_behavior_meteorite_mouse_scroll

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <dt-bindings/zmk/pointing.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

int behavior_input_two_axis_adjust_speed(const struct device *dev, int16_t dx, int16_t dy);

struct behavior_meteorite_mouse_scroll_config {
    const struct device *input_behavior;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {{
    .display_name = "Scroll vector",
    .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
    .range = {.min = 0, .max = -1},
}};

static const struct behavior_parameter_metadata_set param_set = {
    .param1_values_len = ARRAY_SIZE(param1_values),
    .param1_values = param1_values,
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &param_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int update_scroll_speed(struct zmk_behavior_binding *binding, int direction) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_meteorite_mouse_scroll_config *config = dev->config;

    int16_t x = MOVE_X_DECODE(binding->param1);
    int16_t y = MOVE_Y_DECODE(binding->param1);

    return behavior_input_two_axis_adjust_speed(config->input_behavior, direction * x, direction * y);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d scroll 0x%02X", event.position, binding->param1);
    return update_scroll_speed(binding, 1);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d scroll 0x%02X", event.position, binding->param1);
    return update_scroll_speed(binding, -1);
}

static const struct behavior_driver_api behavior_meteorite_mouse_scroll_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define METEORITE_MOUSE_SCROLL_INST(n)                                                            \
    static const struct behavior_meteorite_mouse_scroll_config                                     \
        behavior_meteorite_mouse_scroll_config_##n = {                                             \
            .input_behavior = DEVICE_DT_GET(DT_INST_PHANDLE(n, input_behavior)),                   \
        };                                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_meteorite_mouse_scroll_config_##n,      \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                            &behavior_meteorite_mouse_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_MOUSE_SCROLL_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
