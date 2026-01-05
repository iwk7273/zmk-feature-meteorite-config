#define DT_DRV_COMPAT zmk_behavior_base_layer_save

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/zmk/custom_config.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/custom_feature.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    uint8_t layer_id = binding->param1;
    if (layer_id >= ZMK_KEYMAP_LAYERS_LEN) {
        LOG_WRN("Base layer %u out of range", layer_id);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    zmk_keymap_layer_to(layer_id, false);

    struct zmk_custom_config next = *zmk_custom_config_get();
    next.saved_base_layer = layer_id;
    int ret = zmk_custom_config_set(&next);
    if (ret < 0) {
        LOG_ERR("Failed to set base layer %u (%d)", layer_id, ret);
        return ZMK_BEHAVIOR_OPAQUE;
    }

    ret = zmk_custom_config_apply_op(C_SAVE);
    if (ret < 0) {
        LOG_ERR("Failed to save base layer %u (%d)", layer_id, ret);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_base_layer_save_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define BSL_INST(n)                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                               \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                 \
                            &behavior_base_layer_save_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BSL_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
