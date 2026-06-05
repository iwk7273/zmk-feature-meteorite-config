/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_meteorite_ball_profile

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <stdlib.h>

#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

/* Pulled in for the ball profile enums/constants; the getter calls themselves
 * are guarded by CONFIG_ZMK_CUSTOM_CONFIG below. */
#include <zmk/custom_feature.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Hold time of a fired tap, and the minimum spacing between taps. The hold must
 * be comfortably shorter than the cooldown so the release is always processed
 * before the next press can be queued (avoids the os-key "pressed twice" guard). */
#define BALL_TAP_HOLD_MS 15
#define BALL_COOLDOWN_MS 80

/* Number of fixed action profiles laid out in the `bindings` array
 * (BROWSER, DESKTOP, WINDOW), each contributing 4 direction bindings. */
#define BALL_FIXED_PROFILE_COUNT 3
#define BALL_FIXED_BINDINGS_LEN (BALL_FIXED_PROFILE_COUNT * ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS)

struct meteorite_ball_profile_config {
    uint8_t index;
    size_t processors_len;
    const struct zmk_input_processor_entry *processors;
    size_t fixed_bindings_len;
    const struct zmk_behavior_binding *fixed_bindings;
};

struct meteorite_ball_profile_data {
    int32_t acc_x;
    int32_t acc_y;
    int64_t last_fire_ms;
};

static int run_scroll_chain(const struct meteorite_ball_profile_config *cfg,
                            struct input_event *event,
                            struct zmk_input_processor_state *state) {
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

static void ball_fire(const struct meteorite_ball_profile_config *cfg, uint8_t profile,
                      uint8_t direction, struct zmk_input_processor_state *state) {
    struct zmk_behavior_binding binding = {0};

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    if (profile == ZMK_BALL_PROFILE_USER1) {
        const struct zmk_custom_config_ball_binding *u = zmk_custom_config_user1_binding(direction);
        if (u == NULL || u->behavior_local_id == 0) {
            return; /* no-op direction */
        }
        const char *name = zmk_behavior_find_behavior_name_from_local_id(u->behavior_local_id);
        if (name == NULL) {
            LOG_WRN("ball USER1 dir=%u local_id=%u unresolved; treating as no-op", direction,
                    u->behavior_local_id);
            return;
        }
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
        binding.local_id = u->behavior_local_id;
#endif
        binding.behavior_dev = name;
        binding.param1 = u->param1;
        binding.param2 = u->param2;
    } else
#endif /* CONFIG_ZMK_CUSTOM_CONFIG */
    {
        if (profile < ZMK_BALL_PROFILE_BROWSER || profile > ZMK_BALL_PROFILE_WINDOW) {
            return;
        }
        size_t slot = (size_t)(profile - ZMK_BALL_PROFILE_BROWSER);
        size_t idx = slot * ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS + direction;
        if (idx >= cfg->fixed_bindings_len) {
            return;
        }
        binding = cfg->fixed_bindings[idx];
        if (binding.behavior_dev == NULL) {
            return;
        }
    }

    struct zmk_behavior_binding_event ev = {
        .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(state->input_device_index,
                                                                     cfg->index),
        .timestamp = k_uptime_get(),
    };
    /* Synthesize a tap: press (held briefly) then release. The os-key behavior
     * treats a resolved keycode of 0 as a no-op, so OS-specific directions can
     * be configured with a 0 param for the OS that has no action. */
    zmk_behavior_queue_add(&ev, binding, true, BALL_TAP_HOLD_MS);
    zmk_behavior_queue_add(&ev, binding, false, 0);
}

static int ball_action(const struct meteorite_ball_profile_config *cfg,
                       struct meteorite_ball_profile_data *data, struct input_event *event,
                       uint8_t profile, struct zmk_input_processor_state *state) {
    if (event->code == INPUT_REL_X) {
        data->acc_x += event->value;
    } else if (event->code == INPUT_REL_Y) {
        data->acc_y += event->value;
    } else {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    event->value = 0; /* consume the motion so it never reaches motion_scaler */

    int64_t now = k_uptime_get();
    if (now - data->last_fire_ms < BALL_COOLDOWN_MS) {
        return ZMK_INPUT_PROC_STOP;
    }

    int32_t threshold = 1;
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    threshold = (int32_t)zmk_custom_config_ball_threshold();
#endif
    if (threshold <= 0) {
        threshold = 1;
    }

    bool x_trig = abs(data->acc_x) >= threshold;
    bool y_trig = abs(data->acc_y) >= threshold;
    uint8_t direction;
    bool fire = false;

    /* Dominant-axis selection mirrors meteorite_xy_clipper's hysteresis. */
    if (y_trig && (!x_trig || (abs(data->acc_y) * 2) >= abs(data->acc_x))) {
        direction = (data->acc_y > 0) ? ZMK_BALL_DIR_DOWN : ZMK_BALL_DIR_UP;
        data->acc_y -= (data->acc_y > 0) ? threshold : -threshold;
        data->acc_x = 0;
        fire = true;
    } else if (x_trig) {
        direction = (data->acc_x > 0) ? ZMK_BALL_DIR_RIGHT : ZMK_BALL_DIR_LEFT;
        data->acc_x -= (data->acc_x > 0) ? threshold : -threshold;
        data->acc_y = 0;
        fire = true;
    }

    if (fire) {
        LOG_DBG("ball action profile=%u dir=%u thr=%d", profile, direction, threshold);
        ball_fire(cfg, profile, direction, state);
        data->last_fire_ms = now;
    }
    return ZMK_INPUT_PROC_STOP;
}

static int meteorite_ball_profile_handle_event(const struct device *dev, struct input_event *event,
                                               uint32_t param1, uint32_t param2,
                                               struct zmk_input_processor_state *state) {
    const struct meteorite_ball_profile_config *cfg = dev->config;
    struct meteorite_ball_profile_data *data = dev->data;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint8_t profile = ZMK_BALL_PROFILE_OFF;
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    profile = zmk_custom_config_active_profile();
#endif

    switch (profile) {
    case ZMK_BALL_PROFILE_SCROLL:
        data->acc_x = 0;
        data->acc_y = 0;
        return run_scroll_chain(cfg, event, state);
    case ZMK_BALL_PROFILE_BROWSER:
    case ZMK_BALL_PROFILE_DESKTOP:
    case ZMK_BALL_PROFILE_WINDOW:
    case ZMK_BALL_PROFILE_USER1:
        return ball_action(cfg, data, event, profile, state);
    case ZMK_BALL_PROFILE_OFF:
    default:
        /* OFF (and the reserved APP value): pass straight through to the next
         * processor in the listener chain (motion_scaler). */
        data->acc_x = 0;
        data->acc_y = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }
}

static struct zmk_input_processor_driver_api meteorite_ball_profile_driver_api = {
    .handle_event = meteorite_ball_profile_handle_event,
};

#define METEORITE_BALL_PROFILE_PROCESSORS(n)                                                       \
    COND_CODE_1(                                                                                   \
        DT_NODE_HAS_PROP(DT_DRV_INST(n), input_processors),                                        \
        ({LISTIFY(DT_PROP_LEN(DT_DRV_INST(n), input_processors),                                   \
                  ZMK_INPUT_PROCESSOR_ENTRY_AT_IDX, (, ), DT_DRV_INST(n))}),                       \
        ({}))

#define METEORITE_BALL_PROFILE_INST(n)                                                             \
    static const struct zmk_input_processor_entry meteorite_ball_profile_processors_##n[] =        \
        METEORITE_BALL_PROFILE_PROCESSORS(n);                                                      \
    static const struct zmk_behavior_binding meteorite_ball_profile_bindings_##n[] = {             \
        LISTIFY(DT_INST_PROP_LEN(n, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))}; \
    BUILD_ASSERT(ARRAY_SIZE(meteorite_ball_profile_bindings_##n) == BALL_FIXED_BINDINGS_LEN,       \
                 "ball profile router needs exactly 12 fixed bindings "                            \
                 "(BROWSER/DESKTOP/WINDOW x LEFT/RIGHT/UP/DOWN)");                                 \
    static struct meteorite_ball_profile_data meteorite_ball_profile_data_##n = {                  \
        .acc_x = 0,                                                                                \
        .acc_y = 0,                                                                                \
        .last_fire_ms = 0,                                                                         \
    };                                                                                             \
    static const struct meteorite_ball_profile_config meteorite_ball_profile_config_##n = {        \
        .index = n,                                                                                \
        .processors_len = DT_PROP_LEN_OR(DT_DRV_INST(n), input_processors, 0),                     \
        .processors = meteorite_ball_profile_processors_##n,                                       \
        .fixed_bindings_len = DT_INST_PROP_LEN(n, bindings),                                       \
        .fixed_bindings = meteorite_ball_profile_bindings_##n,                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_ball_profile_data_##n,                         \
                          &meteorite_ball_profile_config_##n, POST_KERNEL,                         \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &meteorite_ball_profile_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_BALL_PROFILE_INST)
