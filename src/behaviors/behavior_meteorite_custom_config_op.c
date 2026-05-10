/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_custom_config

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/custom_config.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {
        .display_name = "Select config op",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = 0,
    },
#define CUSTOM_CONFIG_PARAM_VALUE_METADATA(id, display)                                           \
    {                                                                                             \
        .display_name = display, .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = id,        \
    },
#define CUSTOM_CONFIG_PARAM_VALUE_HIDDEN(id, display)
#define CUSTOM_CONFIG_OP(id, code, name, kind, field, limit, value, metadata, display)            \
    CUSTOM_CONFIG_PARAM_VALUE_##metadata(id, display)
#include <zmk/custom_config_ops.def>
#undef CUSTOM_CONFIG_OP
#undef CUSTOM_CONFIG_PARAM_VALUE_HIDDEN
#undef CUSTOM_CONFIG_PARAM_VALUE_METADATA
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

    if (binding->param1 == 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int ret = zmk_custom_config_apply_op(binding->param1);
    if (ret < 0) {
        LOG_ERR("Failed to apply custom config op %u (%d)", binding->param1, ret);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_custom_feature_toggle_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

#define CFT_INST(n)                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                               \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                 \
                            &behavior_custom_feature_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CFT_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
