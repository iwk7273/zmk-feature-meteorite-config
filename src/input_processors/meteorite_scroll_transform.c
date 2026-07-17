#define DT_DRV_COMPAT zmk_input_processor_meteorite_scroll_transform

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include <zmk/meteorite_scroll_transform.h>
#include <zmk/custom_feature.h>

LOG_MODULE_REGISTER(meteorite_scroll_transform, CONFIG_ZMK_LOG_LEVEL);

#define Q16_ONE 65536
#define SCROLL_STOP_RESET_MS 120
#define SCROLL_DT_MIN_MS 1
#define SCROLL_DT_MAX_MS 32
#define SCROLL_GAIN_RISE_TAU_MS 16
#define SCROLL_GAIN_FALL_TAU_MS 8

enum scroll_axis {
    SCROLL_AXIS_NONE,
    SCROLL_AXIS_X,
    SCROLL_AXIS_Y,
};

struct scroll_gain_point {
    uint16_t speed_mm_s;
    int32_t gain_q16;
};

/* Fixed Adaptive curve. It stays at 1x through 30 mm/s, then rises
 * monotonically to 3x at 180 mm/s. Integer interpolation keeps the per-frame
 * path deterministic and avoids powf(). */
static const struct scroll_gain_point adaptive_gain_lut[] = {
    {0, Q16_ONE},
    {30, Q16_ONE},
    {50, Q16_ONE * 115 / 100},
    {80, Q16_ONE * 145 / 100},
    {110, Q16_ONE * 185 / 100},
    {145, Q16_ONE * 240 / 100},
    {180, Q16_ONE * 3},
};

struct meteorite_scroll_transform_data {
    int64_t frame_x;
    int64_t frame_y;
    int64_t remainder_x_q16;
    int64_t remainder_y_q16;
    int32_t gain_q16;
    int64_t last_frame_ms;
    int32_t last_threshold;
    uint16_t last_cpi;
    uint8_t last_mode;
    int8_t last_invert_x;
    int8_t last_invert_y;
    int8_t last_direction;
    uint8_t axis;
    atomic_val_t reset_generation;
    bool config_initialized;
};

struct meteorite_scroll_transform_config {
    int32_t threshold;
    int32_t invert_x;
    int32_t invert_y;
    int32_t scaling_mode;
    int32_t cpi;
    int32_t default_dt_ms;
};

static atomic_t scroll_reset_generation;

void zmk_meteorite_scroll_transform_reset_all(void) {
    atomic_inc(&scroll_reset_generation);
}

static void reset_motion_state(struct meteorite_scroll_transform_data *data,
                               atomic_val_t generation) {
    data->frame_x = 0;
    data->frame_y = 0;
    data->remainder_x_q16 = 0;
    data->remainder_y_q16 = 0;
    data->gain_q16 = Q16_ONE;
    data->last_frame_ms = 0;
    data->last_direction = 0;
    data->axis = SCROLL_AXIS_NONE;
    data->reset_generation = generation;
}

static int32_t adaptive_gain_q16(uint32_t speed_mm_s) {
    if (speed_mm_s <= adaptive_gain_lut[0].speed_mm_s) {
        return adaptive_gain_lut[0].gain_q16;
    }

    for (size_t i = 1; i < ARRAY_SIZE(adaptive_gain_lut); i++) {
        const struct scroll_gain_point *lo = &adaptive_gain_lut[i - 1];
        const struct scroll_gain_point *hi = &adaptive_gain_lut[i];
        if (speed_mm_s <= hi->speed_mm_s) {
            uint32_t span = hi->speed_mm_s - lo->speed_mm_s;
            uint32_t offset = speed_mm_s - lo->speed_mm_s;
            int64_t gain_span = (int64_t)hi->gain_q16 - lo->gain_q16;
            return lo->gain_q16 + (int32_t)(gain_span * offset / span);
        }
    }

    return adaptive_gain_lut[ARRAY_SIZE(adaptive_gain_lut) - 1].gain_q16;
}

static int32_t smooth_gain_q16(int32_t current, int32_t target, int32_t dt_ms) {
    int32_t tau_ms = target >= current ? SCROLL_GAIN_RISE_TAU_MS : SCROLL_GAIN_FALL_TAU_MS;
    int32_t alpha_q16 = (int32_t)((int64_t)dt_ms * Q16_ONE / (tau_ms + dt_ms));
    int32_t next = current + (int32_t)((int64_t)(target - current) * alpha_q16 / Q16_ONE);
    return CLAMP(next, Q16_ONE, Q16_ONE * 3);
}

static uint32_t physical_speed_mm_s(int64_t dx, int64_t dy, uint16_t cpi, int32_t dt_ms) {
    float x = (float)dx;
    float y = (float)dy;
    float magnitude = sqrtf(x * x + y * y);
    float speed = magnitude * 25400.0f /
                  ((float)(cpi > 0 ? cpi : 1) * (float)(dt_ms > 0 ? dt_ms : 1));

    if (!isfinite(speed) || speed <= 0.0f) {
        return 0;
    }
    if (speed >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)lrintf(speed);
}

static enum scroll_axis choose_axis(struct meteorite_scroll_transform_data *data) {
    uint64_t abs_x = (uint64_t)(data->frame_x < 0 ? -data->frame_x : data->frame_x);
    uint64_t abs_y = (uint64_t)(data->frame_y < 0 ? -data->frame_y : data->frame_y);

    if (abs_x == 0 && abs_y == 0) {
        return SCROLL_AXIS_NONE;
    }

    /* Keep the previous axis until the other is at least 20% stronger. This
     * prevents diagonal motion from alternating wheel axes every frame. */
    if (data->axis == SCROLL_AXIS_X && abs_x > 0 && abs_y * 5 <= abs_x * 6) {
        return SCROLL_AXIS_X;
    }
    if (data->axis == SCROLL_AXIS_Y && abs_y > 0 && abs_x * 5 <= abs_y * 6) {
        return SCROLL_AXIS_Y;
    }

    /* Preserve the legacy vertical bias when no axis is locked. */
    return abs_y > 0 && abs_y * 2 >= abs_x ? SCROLL_AXIS_Y : SCROLL_AXIS_X;
}

static int32_t emit_steps(int64_t *remainder_q16, int64_t delta, int32_t gain_q16,
                          int32_t threshold) {
    int64_t threshold_q16 = (int64_t)MAX(threshold, 1) * Q16_ONE;
    int64_t accumulated = *remainder_q16 + delta * gain_q16;
    int64_t steps = accumulated / threshold_q16;

    /* HID wheel accumulation is int16_t. Discard only impossible excess whole
     * steps while preserving the true sub-step remainder; never bank a backlog
     * that could scroll after the physical input has stopped. */
    int32_t emitted = (int32_t)CLAMP(steps, INT16_MIN, INT16_MAX);
    *remainder_q16 = accumulated % threshold_q16;
    return emitted;
}

static void current_config(const struct meteorite_scroll_transform_config *config,
                           int32_t *threshold, bool *invert_x, bool *invert_y, uint8_t *mode,
                           uint16_t *cpi) {
    *threshold = MAX(config->threshold, 1);
    *invert_x = config->invert_x != 0;
    *invert_y = config->invert_y != 0;
    *mode = config->scaling_mode != 0 ? ZMK_SCROLL_SCALING_MODE_ADAPTIVE
                                      : ZMK_SCROLL_SCALING_MODE_LINEAR;
    *cpi = (uint16_t)(config->cpi > 0 ? config->cpi : 1);

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    *threshold = MAX((int32_t)zmk_custom_config_scroll_div_value(), 1);
    *invert_x = zmk_custom_config_scroll_h_rev();
    *invert_y = zmk_custom_config_scroll_v_rev();
    *mode = zmk_custom_config_scroll_scaling_mode();
    uint16_t custom_cpi = zmk_custom_config_cpi_value();
    *cpi = custom_cpi > 0 ? custom_cpi : 1;
#endif
}

static bool config_changed(struct meteorite_scroll_transform_data *data, int32_t threshold,
                           bool invert_x, bool invert_y, uint8_t mode, uint16_t cpi) {
    if (!data->config_initialized) {
        return true;
    }
    return data->last_threshold != threshold || data->last_invert_x != invert_x ||
           data->last_invert_y != invert_y || data->last_mode != mode || data->last_cpi != cpi;
}

static void remember_config(struct meteorite_scroll_transform_data *data, int32_t threshold,
                            bool invert_x, bool invert_y, uint8_t mode, uint16_t cpi) {
    data->last_threshold = threshold;
    data->last_invert_x = invert_x;
    data->last_invert_y = invert_y;
    data->last_mode = mode;
    data->last_cpi = cpi;
    data->config_initialized = true;
}

static int meteorite_scroll_transform_handle_event(const struct device *dev,
                                                   struct input_event *event, uint32_t param1,
                                                   uint32_t param2,
                                                   struct zmk_input_processor_state *state) {
    struct meteorite_scroll_transform_data *data = dev->data;
    const struct meteorite_scroll_transform_config *config = dev->config;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t threshold;
    bool invert_x;
    bool invert_y;
    uint8_t mode;
    uint16_t cpi;
    current_config(config, &threshold, &invert_x, &invert_y, &mode, &cpi);

    atomic_val_t generation = atomic_get(&scroll_reset_generation);
    int64_t now = k_uptime_get();
    bool stopped = data->last_frame_ms > 0 && now - data->last_frame_ms > SCROLL_STOP_RESET_MS;
    if (generation != data->reset_generation || stopped ||
        config_changed(data, threshold, invert_x, invert_y, mode, cpi)) {
        reset_motion_state(data, generation);
        remember_config(data, threshold, invert_x, invert_y, mode, cpi);
    }

    if (event->code == INPUT_REL_X) {
        data->frame_x += event->value;
        event->code = INPUT_REL_HWHEEL;
    } else {
        data->frame_y += event->value;
        event->code = INPUT_REL_WHEEL;
    }
    event->value = 0;

    if (!event->sync) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t dt_ms = config->default_dt_ms > 0 ? config->default_dt_ms : 12;
    if (data->last_frame_ms > 0) {
        dt_ms = (int32_t)(now - data->last_frame_ms);
    }
    dt_ms = CLAMP(dt_ms, SCROLL_DT_MIN_MS, SCROLL_DT_MAX_MS);

    enum scroll_axis axis = choose_axis(data);
    if (axis == SCROLL_AXIS_NONE) {
        data->frame_x = 0;
        data->frame_y = 0;
        data->last_frame_ms = now;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t delta = axis == SCROLL_AXIS_X ? data->frame_x : data->frame_y;
    int8_t direction = delta < 0 ? -1 : 1;
    if (data->last_direction != 0 && direction != data->last_direction) {
        data->remainder_x_q16 = 0;
        data->remainder_y_q16 = 0;
        data->gain_q16 = Q16_ONE;
    }

    uint32_t speed_mm_s = physical_speed_mm_s(data->frame_x, data->frame_y, cpi, dt_ms);
    if (mode == ZMK_SCROLL_SCALING_MODE_ADAPTIVE) {
        data->gain_q16 = smooth_gain_q16(data->gain_q16, adaptive_gain_q16(speed_mm_s), dt_ms);
    } else {
        data->gain_q16 = Q16_ONE;
    }

    int32_t steps;
    if (axis == SCROLL_AXIS_X) {
        data->remainder_y_q16 = 0;
        steps = emit_steps(&data->remainder_x_q16, delta, data->gain_q16, threshold);
        event->code = INPUT_REL_HWHEEL;
        event->value = invert_x ? -steps : steps;
    } else {
        data->remainder_x_q16 = 0;
        steps = emit_steps(&data->remainder_y_q16, delta, data->gain_q16, threshold);
        event->code = INPUT_REL_WHEEL;
        event->value = invert_y ? -steps : steps;
    }

    LOG_DBG("scroll mode=%u speed=%u gain_q16=%d axis=%u delta=%lld steps=%d rem=(%lld,%lld)",
            mode, speed_mm_s, data->gain_q16, axis, delta, event->value,
            data->remainder_x_q16, data->remainder_y_q16);

    data->frame_x = 0;
    data->frame_y = 0;
    data->last_frame_ms = now;
    data->last_direction = direction;
    data->axis = axis;
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api meteorite_scroll_transform_driver_api = {
    .handle_event = meteorite_scroll_transform_handle_event,
};

#define METEORITE_SCROLL_TRANSFORM_INST(n)                                                        \
    static struct meteorite_scroll_transform_data meteorite_scroll_transform_data_##n = {         \
        .gain_q16 = Q16_ONE,                                                                       \
        .last_threshold = INT32_MIN,                                                               \
        .last_invert_x = -1,                                                                       \
        .last_invert_y = -1,                                                                       \
    };                                                                                             \
    static const struct meteorite_scroll_transform_config meteorite_scroll_transform_config_##n = { \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .invert_x = DT_INST_PROP(n, invert_x),                                                     \
        .invert_y = DT_INST_PROP(n, invert_y),                                                     \
        .scaling_mode = DT_INST_PROP(n, scaling_mode),                                             \
        .cpi = DT_INST_PROP(n, cpi),                                                               \
        .default_dt_ms = DT_INST_PROP(n, default_dt_ms),                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_scroll_transform_data_##n,                     \
                          &meteorite_scroll_transform_config_##n, POST_KERNEL,                     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                          &meteorite_scroll_transform_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_SCROLL_TRANSFORM_INST)
