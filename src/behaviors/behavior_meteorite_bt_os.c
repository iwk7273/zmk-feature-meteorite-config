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

static bool param_to_slot_os(uint32_t param, uint8_t *slot, bool *is_mac) {
    if (param < M_BT0_WIN || param > M_BT4_MAC) {
        return false;
    }

    uint32_t idx = param - M_BT0_WIN;
    *slot = (uint8_t)(idx / 2);
    *is_mac = (idx % 2) == 1;
    return true;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {.display_name = "BT0 + Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT0_WIN},
    {.display_name = "BT0 + Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT0_MAC},
    {.display_name = "BT1 + Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT1_WIN},
    {.display_name = "BT1 + Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT1_MAC},
    {.display_name = "BT2 + Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT2_WIN},
    {.display_name = "BT2 + Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT2_MAC},
    {.display_name = "BT3 + Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT3_WIN},
    {.display_name = "BT3 + Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT3_MAC},
    {.display_name = "BT4 + Win", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT4_WIN},
    {.display_name = "BT4 + Mac", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = M_BT4_MAC},
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
    ARG_UNUSED(event);

    uint8_t slot = 0;
    bool is_mac = false;
    if (!param_to_slot_os(binding->param1, &slot, &is_mac)) {
        LOG_ERR("Unknown meteorite bt/os param: %u", binding->param1);
        return -ENOTSUP;
    }

    int ret = zmk_custom_config_apply_op(is_mac ? C_OS_MAC : C_OS_WIN);
    if (ret < 0) {
        LOG_ERR("Failed to set OS mode (%d)", ret);
    }

    ret = zmk_custom_config_apply_op(C_SAVE);
    if (ret < 0) {
        LOG_ERR("Failed to save OS mode (%d)", ret);
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
