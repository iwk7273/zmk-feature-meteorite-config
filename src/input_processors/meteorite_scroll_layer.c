#define DT_DRV_COMPAT zmk_input_processor_meteorite_scroll_layer

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
#include <zmk/custom_feature.h>
#endif
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Bit width of zmk_keymap_layers_state_t (uint32_t); layer indices must fit in it. */
#define SCROLL_LAYER_STATE_BITS 32

struct meteorite_scroll_layer_config {
    size_t processors_len;
    const struct zmk_input_processor_entry *processors;
    uint8_t layer_1;
    uint8_t layer_2;
};

struct meteorite_scroll_layers {
    bool valid;
    uint8_t layer_1;
    uint8_t layer_2;
};

static struct meteorite_scroll_layers
resolve_scroll_layers(const struct meteorite_scroll_layer_config *cfg) {
    uint8_t layer_count = ZMK_KEYMAP_LAYERS_LEN;

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    uint8_t layer_1 = zmk_custom_config_scroll_layer_1();
    uint8_t layer_2 = zmk_custom_config_scroll_layer_2();
#else
    uint8_t layer_1 = cfg->layer_1;
    uint8_t layer_2 = cfg->layer_2;
#endif

    if (layer_count == 0 || layer_count > SCROLL_LAYER_STATE_BITS) {
        return (struct meteorite_scroll_layers){
            .valid = false,
            .layer_1 = layer_1,
            .layer_2 = layer_2,
        };
    }

    return (struct meteorite_scroll_layers){
        .valid = true,
        .layer_1 = layer_1 % layer_count,
        .layer_2 = layer_2 % layer_count,
    };
}

static bool meteorite_scroll_layers_active(struct meteorite_scroll_layers layers) {
    return layers.valid && (zmk_keymap_layer_active(layers.layer_1) ||
                            zmk_keymap_layer_active(layers.layer_2));
}

static int meteorite_scroll_layer_handle_event(const struct device *dev, struct input_event *event,
                                               uint32_t param1, uint32_t param2,
                                               struct zmk_input_processor_state *state) {
    const struct meteorite_scroll_layer_config *cfg = dev->config;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    struct meteorite_scroll_layers layers = resolve_scroll_layers(cfg);
    bool active = meteorite_scroll_layers_active(layers);
    LOG_DBG("meteorite_scroll_layer active=%d layer_1=%u layer_2=%u code=%u val=%d sync=%d",
            active, layers.layer_1, layers.layer_2, event->code, event->value, event->sync);
    if (!active) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (size_t i = 0; i < cfg->processors_len; i++) {
        const struct zmk_input_processor_entry *proc = &cfg->processors[i];
        int ret = zmk_input_processor_handle_event(proc->dev, event, proc->param1, proc->param2,
                                                   state);
        if (ret != ZMK_INPUT_PROC_CONTINUE) {
            return ret;
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api meteorite_scroll_layer_driver_api = {
    .handle_event = meteorite_scroll_layer_handle_event,
};

#define METEORITE_SCROLL_LAYER_PROCESSORS(n)                                                       \
    COND_CODE_1(                                                                                   \
        DT_NODE_HAS_PROP(DT_DRV_INST(n), input_processors),                                       \
        ({LISTIFY(DT_PROP_LEN(DT_DRV_INST(n), input_processors),                                   \
                  ZMK_INPUT_PROCESSOR_ENTRY_AT_IDX, (, ), DT_DRV_INST(n))}),                       \
        ({}))

#define METEORITE_SCROLL_LAYER_INST(n)                                                            \
    static const struct zmk_input_processor_entry meteorite_scroll_layer_processors_##n[] =        \
        METEORITE_SCROLL_LAYER_PROCESSORS(n);                                                      \
    static const struct meteorite_scroll_layer_config meteorite_scroll_layer_config_##n = {        \
        .processors_len = DT_PROP_LEN_OR(DT_DRV_INST(n), input_processors, 0),                     \
        .processors = meteorite_scroll_layer_processors_##n,                                       \
        .layer_1 = DT_INST_PROP_OR(n, layer_1, 0),                                                  \
        .layer_2 = DT_INST_PROP_OR(n, layer_2, 0),                                                  \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &meteorite_scroll_layer_config_##n,                  \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &meteorite_scroll_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_SCROLL_LAYER_INST)
