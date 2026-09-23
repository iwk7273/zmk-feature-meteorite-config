#define DT_DRV_COMPAT zmk_behavior_meteorite_os_macro

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) && IS_ENABLED(CONFIG_ZMK_MACRO_SETTINGS)

#include <zmk/user_macros.h>

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

#define METEORITE_OS_MACRO_PARAM_VALUE(i, _)                                                       \
    {                                                                                              \
        .display_name = "User Macro " STRINGIFY(UTIL_INC(i)),                                      \
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,                                               \
        .value = (UTIL_INC(i)),                                                                    \
    }

#define METEORITE_OS_MACRO_LISTIFY(len, F, sep) LISTIFY(len, F, sep)

static const struct behavior_parameter_value_metadata macro_values[] = {
    {
        .display_name = "None",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = 0,
    },
    METEORITE_OS_MACRO_LISTIFY(CONFIG_ZMK_MACRO_SETTINGS_MAX_MACROS, METEORITE_OS_MACRO_PARAM_VALUE,
                               (, )),
};

static const struct behavior_parameter_metadata_set param_set = {
    .param1_values = macro_values,
    .param1_values_len = ARRAY_SIZE(macro_values),
    .param2_values = macro_values,
    .param2_values_len = ARRAY_SIZE(macro_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &param_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    uint32_t param = zmk_custom_config_os_is_mac() ? binding->param2 : binding->param1;

    /* 0 means "no action for this OS". */
    if (param == 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    if (param > CONFIG_ZMK_MACRO_SETTINGS_MAX_MACROS) {
        LOG_ERR("Unknown meteorite os macro param: %u", param);
        return -EINVAL;
    }

    int ret = zmk_user_macro_queue((uint8_t)(param - 1), &event);
    return ret < 0 ? ret : ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_meteorite_os_macro_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MOSM_INST(n)                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_meteorite_os_macro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MOSM_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) && IS_ENABLED(CONFIG_ZMK_MACRO_SETTINGS) */
