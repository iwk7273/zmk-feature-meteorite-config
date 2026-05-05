#define DT_DRV_COMPAT zmk_behavior_meteorite_mouse_scroll

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define METEORITE_SCRL_RIGHT 0x00C80000U
#define METEORITE_SCRL_LEFT 0xFF380000U
#define METEORITE_SCRL_UP 0x000000C8U
#define METEORITE_SCRL_DOWN 0x0000FF38U

struct behavior_meteorite_mouse_scroll_config {
    const char *input_behavior_name;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {
        .display_name = "Scroll Right",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = METEORITE_SCRL_RIGHT,
    },
    {
        .display_name = "Scroll Left",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = METEORITE_SCRL_LEFT,
    },
    {
        .display_name = "Scroll Up",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = METEORITE_SCRL_UP,
    },
    {
        .display_name = "Scroll Down",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = METEORITE_SCRL_DOWN,
    },
};

static const struct behavior_parameter_metadata_set param_set = {
    .param1_values_len = ARRAY_SIZE(param1_values),
    .param1_values = param1_values,
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &param_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int invoke_scroll_behavior(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event, bool pressed) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_meteorite_mouse_scroll_config *config = dev->config;

    struct zmk_behavior_binding scroll_binding = {
        .behavior_dev = config->input_behavior_name,
        .param1 = binding->param1,
        .param2 = binding->param2,
    };

    LOG_DBG("position %d scroll 0x%02X pressed=%d", event.position, binding->param1, pressed);

    return zmk_behavior_invoke_binding(&scroll_binding, event, pressed);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return invoke_scroll_behavior(binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return invoke_scroll_behavior(binding, event, false);
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
            .input_behavior_name = DEVICE_DT_NAME(DT_INST_PHANDLE(n, input_behavior)),             \
        };                                                                                         \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_meteorite_mouse_scroll_config_##n,      \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                            &behavior_meteorite_mouse_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_MOUSE_SCROLL_INST)

#endif // DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
