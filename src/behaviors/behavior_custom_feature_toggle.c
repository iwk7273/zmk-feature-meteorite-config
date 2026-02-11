/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_custom_config

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/custom_config.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata param1_values[] = {
    {
        .display_name = "CPI Up - トラックボールの感度（CPI）を200上げる",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_CPI_UP,
    },
    {
        .display_name = "CPI Down - トラックボールの感度（CPI）を200下げる",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_CPI_DN,
    },
    {
        .display_name = "Scroll Div Up - スクロール分解能を上げる（感度を下げる）",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SDIV_UP,
    },
    {
        .display_name = "Scroll Div Down - スクロール分解能を下げる（感度を上げる）",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SDIV_DN,
    },
    {
        .display_name = "Rotation Up - トラックボールの軸を時計回りに10°回転する（最大70°）",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_ROT_UP,
    },
    {
        .display_name = "Rotation Down - トラックボールの軸を反時計回りに10°回転する（最大-70°）",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_ROT_DN,
    },
    {
        .display_name = "Motion Scaling Toggle - トラックボールのスケーリングモードを切り替える",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SCALE_TOG,
    },
    {
        .display_name = "Scroll H Reverse - 水平スクロールの方向を反転する",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SCRH_TOG,
    },
    {
        .display_name = "Scroll V Reverse - 垂直スクロールの方向を反転する",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SCRV_TOG,
    },
    {
        .display_name = "Scroll Layer 2 Next - スクロールレイヤー2を次へ切り替える",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SCRL2_UP,
    },
    {
        .display_name = "Scroll Scaling Toggle - スクロールのスケーリングモードを切り替える",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SCRL_SCALE_TOG,
    },
    {
        .display_name = "Save Config - 現在のカスタム設定を保存する",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_SAVE,
    },
    {
        .display_name = "Reset Config - カスタム設定を初期値に戻す",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_RESET,
    },
    {
        .display_name = "OS Mode Toggle - Win/Macを切り替える",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
        .value = C_OS_TOG,
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
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

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
