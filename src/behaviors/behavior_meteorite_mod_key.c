#define DT_DRV_COMPAT zmk_behavior_meteorite_mod_key

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/meteorite_custom_keys.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>
#include <zmk/meteorite_os_key.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_meteorite_mod_key_data {
    struct zmk_behavior_binding mod_binding;
    struct zmk_behavior_binding key_binding;
    bool pressed;
};

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

#define METEORITE_OS_KEY_IDENTITY(x) (x)
static const struct behavior_parameter_value_metadata param1_values[] = {
    METEORITE_OS_KEY_METADATA_VALUES_WITH_DEFAULT(0, METEORITE_OS_KEY_IDENTITY),
};
#undef METEORITE_OS_KEY_IDENTITY

static const struct behavior_parameter_value_metadata param2_values[] = {
    {
        .display_name = "Key",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_HID_USAGE,
    },
};

static const struct behavior_parameter_metadata_set param_set = {
    .param1_values = param1_values,
    .param1_values_len = ARRAY_SIZE(param1_values),
    .param2_values = param2_values,
    .param2_values_len = ARRAY_SIZE(param2_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &param_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    struct behavior_meteorite_mod_key_data *data =
        (struct behavior_meteorite_mod_key_data *)zmk_behavior_get_binding(binding->behavior_dev)
            ->data;

    if (data->pressed) {
        LOG_ERR("Can't press the same meteorite mod key twice");
        return -ENOTSUP;
    }

    uint32_t keycode = 0;
    if (!meteorite_os_keycode_for_param(binding->param1, zmk_custom_config_os_is_mac(),
                                        &keycode)) {
        LOG_ERR("Unknown meteorite mod key param: %u", binding->param1);
        return -ENOTSUP;
    }

    data->mod_binding.behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(kp));
    data->mod_binding.param1 = keycode;
    data->mod_binding.param2 = 0;

    data->key_binding.behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(kp));
    data->key_binding.param1 = binding->param2;
    data->key_binding.param2 = 0;

    data->pressed = true;

    int ret = zmk_behavior_invoke_binding(&data->mod_binding, event, true);
    if (ret < 0) {
        data->pressed = false;
        return ret;
    }

    return zmk_behavior_invoke_binding(&data->key_binding, event, true);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    struct behavior_meteorite_mod_key_data *data =
        (struct behavior_meteorite_mod_key_data *)zmk_behavior_get_binding(binding->behavior_dev)
            ->data;

    if (!data->pressed) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int ret = zmk_behavior_invoke_binding(&data->key_binding, event, false);
    int ret_mod = zmk_behavior_invoke_binding(&data->mod_binding, event, false);

    data->pressed = false;

    if (ret < 0) {
        return ret;
    }
    return ret_mod;
}

static const struct behavior_driver_api behavior_meteorite_mod_key_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MMK_INST(n)                                                                             \
    static struct behavior_meteorite_mod_key_data behavior_meteorite_mod_key_data_##n = {};     \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_meteorite_mod_key_data_##n, NULL,           \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                   \
                            &behavior_meteorite_mod_key_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MMK_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
