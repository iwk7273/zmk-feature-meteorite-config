#define DT_DRV_COMPAT zmk_input_processor_scroll_layer

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/custom_feature.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct scroll_layer_config {
    size_t processors_len;
    const struct zmk_input_processor_entry *processors;
};

static bool scroll_layers_active(void) {
    uint8_t layer_count = ZMK_KEYMAP_LAYERS_LEN;

    if (layer_count == 0 || layer_count > 32) {
        return false;
    }

    uint8_t layer_1 = zmk_custom_config_scroll_layer_1() % layer_count;
    uint8_t layer_2 = zmk_custom_config_scroll_layer_2() % layer_count;

    return zmk_keymap_layer_active(layer_1) || zmk_keymap_layer_active(layer_2);
}

static int scroll_layer_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t param1, uint32_t param2,
                                     struct zmk_input_processor_state *state) {
    const struct scroll_layer_config *cfg = dev->config;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (!scroll_layers_active()) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    struct zmk_input_processor_state local_state = {
        .input_device_index = state ? state->input_device_index : 0,
        .remainder = NULL,
    };

    for (size_t i = 0; i < cfg->processors_len; i++) {
        const struct zmk_input_processor_entry *proc = &cfg->processors[i];
        int ret = zmk_input_processor_handle_event(proc->dev, event, proc->param1, proc->param2,
                                                   &local_state);
        switch (ret) {
        case ZMK_INPUT_PROC_CONTINUE:
            continue;
        default:
            return ret;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api scroll_layer_driver_api = {
    .handle_event = scroll_layer_handle_event,
};

#define SCROLL_LAYER_PROCESSORS(n)                                                                 \
    COND_CODE_1(                                                                                   \
        DT_NODE_HAS_PROP(DT_DRV_INST(n), input_processors),                                       \
        ({LISTIFY(DT_PROP_LEN(DT_DRV_INST(n), input_processors),                                   \
                  ZMK_INPUT_PROCESSOR_ENTRY_AT_IDX, (, ), DT_DRV_INST(n))}),                       \
        ({}))

#define SCROLL_LAYER_INST(n)                                                                       \
    static const struct zmk_input_processor_entry scroll_layer_processors_##n[] =                 \
        SCROLL_LAYER_PROCESSORS(n);                                                               \
    static const struct scroll_layer_config scroll_layer_config_##n = {                            \
        .processors_len = DT_PROP_LEN_OR(DT_DRV_INST(n), input_processors, 0),                     \
        .processors = scroll_layer_processors_##n,                                                 \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &scroll_layer_config_##n, POST_KERNEL,             \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &scroll_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_LAYER_INST)
