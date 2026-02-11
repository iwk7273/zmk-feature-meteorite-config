#define DT_DRV_COMPAT zmk_behavior_os_switch

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_os_switch_config {
    struct zmk_behavior_binding win_binding;
    struct zmk_behavior_binding mac_binding;
};

struct behavior_os_switch_data {
    struct zmk_behavior_binding *pressed_binding;
};

static struct zmk_behavior_binding *
select_binding(const struct behavior_os_switch_config *cfg) {
    return zmk_custom_config_os_is_mac() ? (struct zmk_behavior_binding *)&cfg->mac_binding
                                         : (struct zmk_behavior_binding *)&cfg->win_binding;
}

static int on_os_switch_binding_pressed(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_os_switch_config *cfg = dev->config;
    struct behavior_os_switch_data *data = dev->data;

    if (data->pressed_binding != NULL) {
        LOG_ERR("Can't press the same os-switch twice");
        return -ENOTSUP;
    }

    data->pressed_binding = select_binding(cfg);
    return zmk_behavior_invoke_binding(data->pressed_binding, event, true);
}

static int on_os_switch_binding_released(struct zmk_behavior_binding *binding,
                                         struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_os_switch_data *data = dev->data;

    if (data->pressed_binding == NULL) {
        LOG_ERR("OS-switch already released");
        return -ENOTSUP;
    }

    struct zmk_behavior_binding *pressed_binding = data->pressed_binding;
    data->pressed_binding = NULL;
    return zmk_behavior_invoke_binding(pressed_binding, event, false);
}

static const struct behavior_driver_api behavior_os_switch_driver_api = {
    .binding_pressed = on_os_switch_binding_pressed,
    .binding_released = on_os_switch_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define _TRANSFORM_ENTRY(idx, node)                                                                \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(node, bindings, idx)),               \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param1), (0),       \
                              (DT_INST_PHA_BY_IDX(node, bindings, idx, param1))),                  \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param2), (0),       \
                              (DT_INST_PHA_BY_IDX(node, bindings, idx, param2))),                  \
    }

#define OS_SWITCH_INST(n)                                                                          \
    static struct behavior_os_switch_config behavior_os_switch_config_##n = {                     \
        .win_binding = _TRANSFORM_ENTRY(0, n),                                                     \
        .mac_binding = _TRANSFORM_ENTRY(1, n),                                                     \
    };                                                                                             \
    static struct behavior_os_switch_data behavior_os_switch_data_##n = {};                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_os_switch_data_##n,                           \
                            &behavior_os_switch_config_##n, POST_KERNEL,                           \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                            &behavior_os_switch_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OS_SWITCH_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
