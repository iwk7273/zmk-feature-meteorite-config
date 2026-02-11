#define DT_DRV_COMPAT zmk_behavior_meteorite_key_or_custom

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/meteorite_custom_keys.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_meteorite_key_or_custom_data {
    struct zmk_behavior_binding pressed_binding;
    bool pressed;
};

struct behavior_meteorite_key_or_custom_config {
    const char *alt_cmd_tab_behavior_dev;
};

static bool keycode_for_param(uint32_t param, bool is_mac, uint32_t *keycode) {
    switch (param) {
    case M_OS_CTRL_CMD:
        *keycode = is_mac ? LGUI : LCTRL;
        return true;
    case M_OS_ALT_OPT:
        *keycode = LALT;
        return true;
    case M_OS_ALT_CTRL:
        *keycode = is_mac ? LCTRL : LALT;
        return true;
    case M_OS_WIN_CTRL:
        *keycode = is_mac ? LCTRL : LGUI;
        return true;
    case M_OS_WIN_OPT:
        *keycode = is_mac ? LALT : LGUI;
        return true;
    case M_OS_ALT_CMD:
        *keycode = is_mac ? LGUI : LALT;
        return true;
    default:
        return false;
    }
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {
        .display_name = "OS Ctrl/Cmd",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_CTRL_CMD),
    },
    {
        .display_name = "OS Alt/Opt",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_ALT_OPT),
    },
    {
        .display_name = "OS Alt/Ctrl",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_ALT_CTRL),
    },
    {
        .display_name = "OS Win/Ctrl",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_WIN_CTRL),
    },
    {
        .display_name = "OS Win/Opt",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_WIN_OPT),
    },
    {
        .display_name = "OS Alt/Cmd",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM(M_OS_ALT_CMD),
    },
    {
        .display_name = "Alt/Cmd+Tab",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = MCK_TAP_PARAM_ALT_CMD_TAB,
    },
    {
        .display_name = "Key",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_HID_USAGE,
    },
};

static const struct behavior_parameter_metadata_set param_set = {
    .param1_values = param1_values,
    .param1_values_len = ARRAY_SIZE(param1_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &param_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_meteorite_key_or_custom_config *cfg = dev->config;
    struct behavior_meteorite_key_or_custom_data *data =
        (struct behavior_meteorite_key_or_custom_data *)dev->data;

    if (data->pressed) {
        LOG_ERR("Can't press the same meteorite tap key twice");
        return -ENOTSUP;
    }

    uint32_t keycode = 0;
    if (binding->param1 == MCK_TAP_PARAM_ALT_CMD_TAB) {
        if (cfg->alt_cmd_tab_behavior_dev == NULL) {
            LOG_ERR("Alt/Cmd+Tab behavior not configured");
            return -ENOTSUP;
        }

        data->pressed_binding.behavior_dev = cfg->alt_cmd_tab_behavior_dev;
        data->pressed_binding.param1 = 0;
        data->pressed_binding.param2 = 0;
        data->pressed = true;

        return zmk_behavior_invoke_binding(&data->pressed_binding, event, true);
    } else if (binding->param1 >= MCK_TAP_PARAM_BASE) {
        uint32_t custom_param = binding->param1 - MCK_TAP_PARAM_BASE;
        if (!keycode_for_param(custom_param, zmk_custom_config_os_is_mac(), &keycode)) {
            LOG_ERR("Unknown meteorite tap custom param: %u", custom_param);
            return -ENOTSUP;
        }
    } else {
        keycode = binding->param1;
    }

    data->pressed_binding.behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(kp));
    data->pressed_binding.param1 = keycode;
    data->pressed_binding.param2 = 0;
    data->pressed = true;

    return zmk_behavior_invoke_binding(&data->pressed_binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_meteorite_key_or_custom_data *data =
        (struct behavior_meteorite_key_or_custom_data *)dev->data;

    if (!data->pressed) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->pressed = false;
    return zmk_behavior_invoke_binding(&data->pressed_binding, event, false);
}

static const struct behavior_driver_api behavior_meteorite_key_or_custom_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MCK_TAP_ALT_CMD_TAB_DEV(n)                                                                \
    COND_CODE_1(DT_INST_NODE_HAS_PROP(n, alt_cmd_tab),                                             \
                (DEVICE_DT_NAME(DT_INST_PHANDLE(n, alt_cmd_tab))),                                 \
                (NULL))

#define MCK_TAP_INST(n)                                                                           \
    static struct behavior_meteorite_key_or_custom_data behavior_meteorite_key_or_custom_data_##n = {}; \
    static const struct behavior_meteorite_key_or_custom_config                                  \
        behavior_meteorite_key_or_custom_config_##n = {                                           \
            .alt_cmd_tab_behavior_dev = MCK_TAP_ALT_CMD_TAB_DEV(n),                                \
        };                                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_meteorite_key_or_custom_data_##n,            \
                            &behavior_meteorite_key_or_custom_config_##n, POST_KERNEL,           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                 \
                            &behavior_meteorite_key_or_custom_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MCK_TAP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
