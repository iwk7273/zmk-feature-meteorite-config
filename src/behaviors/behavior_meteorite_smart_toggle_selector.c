#define DT_DRV_COMPAT zmk_behavior_meteorite_smart_toggle

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_meteorite_smart_toggle_data {
    struct zmk_behavior_binding pressed_binding;
    bool pressed;
};

struct behavior_meteorite_smart_toggle_config {
    const char *alt_cmd_tab_behavior_dev;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_meteorite_smart_toggle_config *cfg = dev->config;
    struct behavior_meteorite_smart_toggle_data *data =
        (struct behavior_meteorite_smart_toggle_data *)dev->data;

    if (data->pressed) {
        LOG_ERR("Can't press the same meteorite smart toggle twice");
        return -ENOTSUP;
    }

    const char *behavior_dev = cfg->alt_cmd_tab_behavior_dev;

    if (behavior_dev == NULL) {
        LOG_ERR("Meteorite smart toggle behavior not configured");
        return -ENOTSUP;
    }

    data->pressed_binding.behavior_dev = behavior_dev;
    data->pressed_binding.param1 = 0;
    data->pressed_binding.param2 = 0;
    data->pressed = true;

    return zmk_behavior_invoke_binding(&data->pressed_binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_meteorite_smart_toggle_data *data =
        (struct behavior_meteorite_smart_toggle_data *)dev->data;

    if (!data->pressed) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->pressed = false;
    return zmk_behavior_invoke_binding(&data->pressed_binding, event, false);
}

static const struct behavior_driver_api behavior_meteorite_smart_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

#define MST_ALT_CMD_TAB_DEV(n)                                                                    \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, alt_cmd_tab),                                             \
                (DEVICE_DT_NAME(DT_INST_PHANDLE(n, alt_cmd_tab))),                                 \
                (NULL))

#define MST_INST(n)                                                                               \
    static struct behavior_meteorite_smart_toggle_data behavior_meteorite_smart_toggle_data_##n = {}; \
    static const struct behavior_meteorite_smart_toggle_config                                   \
        behavior_meteorite_smart_toggle_config_##n = {                                            \
            .alt_cmd_tab_behavior_dev = MST_ALT_CMD_TAB_DEV(n),                                    \
        };                                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_meteorite_smart_toggle_data_##n,             \
                            &behavior_meteorite_smart_toggle_config_##n, POST_KERNEL,            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                 \
                            &behavior_meteorite_smart_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MST_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
