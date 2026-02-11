#define DT_DRV_COMPAT zmk_behavior_meteorite_bt_os

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/custom_config.h>
#include <dt-bindings/zmk/meteorite_bt_os.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {.display_name = "BT0", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT0},
    {.display_name = "BT1", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT1},
    {.display_name = "BT2", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT2},
    {.display_name = "BT3", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT3},
    {.display_name = "BT4", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT4},
};

static const struct behavior_parameter_value_metadata param2_values[] = {
    {.display_name = "Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_OS_WIN},
    {.display_name = "Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_OS_MAC},
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
    ARG_UNUSED(event);

    uint32_t slot = binding->param1;
    uint32_t os = binding->param2;

    if (slot > M_BT4) {
        LOG_ERR("Invalid BT slot: %u", slot);
        return -ENOTSUP;
    }

    if (os != M_OS_WIN && os != M_OS_MAC) {
        LOG_ERR("Invalid OS selection: %u", os);
        return -ENOTSUP;
    }

    int ret = zmk_custom_config_apply_op(os == M_OS_MAC ? C_OS_MAC : C_OS_WIN);
    if (ret < 0) {
        LOG_ERR("Failed to set OS mode (%d)", ret);
    }

    ret = zmk_ble_prof_select(slot);
    if (ret < 0) {
        LOG_ERR("Failed to select BT profile %u (%d)", slot, ret);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_meteorite_bt_os_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define MBT_INST(n)                                                                              \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                              \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                \
                            &behavior_meteorite_bt_os_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MBT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
