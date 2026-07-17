#define DT_DRV_COMPAT zmk_input_processor_meteorite_motion_scaler

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
#include <stddef.h>
#include <stdint.h>

#include <zmk/custom_feature.h>
#include <zmk/meteorite_motion_scaler.h>

LOG_MODULE_REGISTER(meteorite_motion_scaler, CONFIG_ZMK_LOG_LEVEL);

#define Q16_ONE 65536
#define Q16_GAIN_PERCENT(percent) ((int32_t)((int64_t)Q16_ONE * (percent) / 100))

#define MOTION_STOP_RESET_MS 120
#define MOTION_DT_MIN_MS 1
#define MOTION_DT_MAX_MS 32

struct pointer_gain_point {
    uint16_t speed_mm_s;
    int32_t gain_q16;
};

struct pointer_profile_curve {
    const struct pointer_gain_point *points;
    size_t point_count;
    uint16_t rise_tau_ms;
    uint16_t fall_tau_ms;
};

static const struct pointer_gain_point standard_gain_lut[] = {
    {0, Q16_GAIN_PERCENT(40)},   {15, Q16_GAIN_PERCENT(41)},
    {30, Q16_GAIN_PERCENT(100)}, {50, Q16_GAIN_PERCENT(175)},
    {80, Q16_GAIN_PERCENT(235)}, {120, Q16_GAIN_PERCENT(275)},
    {160, Q16_GAIN_PERCENT(300)},
};

static const struct pointer_gain_point stable_gain_lut[] = {
    {0, Q16_GAIN_PERCENT(30)},    {25, Q16_GAIN_PERCENT(45)},
    {50, Q16_GAIN_PERCENT(100)},  {90, Q16_GAIN_PERCENT(125)},
    {140, Q16_GAIN_PERCENT(150)}, {200, Q16_GAIN_PERCENT(170)},
    {250, Q16_GAIN_PERCENT(180)},
};

static const struct pointer_gain_point responsive_gain_lut[] = {
    {0, Q16_GAIN_PERCENT(55)},   {10, Q16_GAIN_PERCENT(70)},
    {20, Q16_GAIN_PERCENT(100)}, {40, Q16_GAIN_PERCENT(175)},
    {70, Q16_GAIN_PERCENT(250)}, {95, Q16_GAIN_PERCENT(300)},
    {120, Q16_GAIN_PERCENT(340)},
};

static const struct pointer_gain_point wide_gain_lut[] = {
    {0, Q16_GAIN_PERCENT(45)},   {12, Q16_GAIN_PERCENT(60)},
    {25, Q16_GAIN_PERCENT(100)}, {50, Q16_GAIN_PERCENT(180)},
    {90, Q16_GAIN_PERCENT(280)}, {135, Q16_GAIN_PERCENT(360)},
    {180, Q16_GAIN_PERCENT(420)},
};

#define POINTER_PROFILE_CURVE(lut, rise, fall)                                                     \
    {                                                                                              \
        .points = (lut), .point_count = ARRAY_SIZE(lut), .rise_tau_ms = (rise),                   \
        .fall_tau_ms = (fall),                                                                     \
    }

static const struct pointer_profile_curve pointer_profile_curves[ZMK_POINTER_PROFILE_COUNT] = {
    [ZMK_POINTER_PROFILE_STANDARD] = POINTER_PROFILE_CURVE(standard_gain_lut, 18, 9),
    [ZMK_POINTER_PROFILE_STABLE] = POINTER_PROFILE_CURVE(stable_gain_lut, 24, 12),
    [ZMK_POINTER_PROFILE_RESPONSIVE] = POINTER_PROFILE_CURVE(responsive_gain_lut, 12, 6),
    [ZMK_POINTER_PROFILE_WIDE] = POINTER_PROFILE_CURVE(wide_gain_lut, 18, 9),
};

struct meteorite_motion_scaler_data {
    int32_t remainder_x_q16;
    int32_t remainder_y_q16;
    int32_t gain_q16;
    int64_t frame_x;
    int64_t frame_y;
    int64_t previous_frame_x;
    int64_t previous_frame_y;
    int64_t last_frame_ms;
    uint16_t last_cpi;
    uint8_t last_profile;
    atomic_val_t reset_generation;
    bool last_enabled;
    bool config_initialized;
};

struct meteorite_motion_scaler_config {
    /* Fallbacks used only when CONFIG_ZMK_CUSTOM_CONFIG is disabled. */
    int32_t scaling_mode;
    int32_t pointer_profile;
    int32_t cpi;
    int32_t default_dt_ms;
};

static atomic_t motion_reset_generation;

void zmk_meteorite_motion_scaler_reset_all(void) { atomic_inc(&motion_reset_generation); }

static uint8_t sanitize_profile(int32_t profile) {
    return profile >= 0 && profile < ZMK_POINTER_PROFILE_COUNT
               ? (uint8_t)profile
               : ZMK_POINTER_PROFILE_STANDARD;
}

static const struct pointer_profile_curve *profile_curve(uint8_t profile) {
    return &pointer_profile_curves[sanitize_profile(profile)];
}

static int32_t profile_min_gain_q16(uint8_t profile) {
    const struct pointer_profile_curve *curve = profile_curve(profile);
    return curve->points[0].gain_q16;
}

static void reset_motion_state(struct meteorite_motion_scaler_data *data,
                               atomic_val_t generation, uint8_t profile) {
    data->remainder_x_q16 = 0;
    data->remainder_y_q16 = 0;
    data->gain_q16 = profile_min_gain_q16(profile);
    data->frame_x = 0;
    data->frame_y = 0;
    data->previous_frame_x = 0;
    data->previous_frame_y = 0;
    data->last_frame_ms = 0;
    data->reset_generation = generation;
}

static int32_t gain_for_speed_q16(uint8_t profile, uint32_t speed_mm_s) {
    const struct pointer_profile_curve *curve = profile_curve(profile);
    const struct pointer_gain_point *points = curve->points;

    if (speed_mm_s <= points[0].speed_mm_s) {
        return points[0].gain_q16;
    }

    for (size_t i = 1; i < curve->point_count; i++) {
        const struct pointer_gain_point *low = &points[i - 1];
        const struct pointer_gain_point *high = &points[i];
        if (speed_mm_s <= high->speed_mm_s) {
            uint32_t span = high->speed_mm_s - low->speed_mm_s;
            uint32_t offset = speed_mm_s - low->speed_mm_s;
            int64_t gain_span = (int64_t)high->gain_q16 - low->gain_q16;
            return low->gain_q16 + (int32_t)(gain_span * offset / span);
        }
    }

    return points[curve->point_count - 1].gain_q16;
}

static int32_t smooth_gain_q16(uint8_t profile, int32_t current, int32_t target,
                               int32_t dt_ms) {
    const struct pointer_profile_curve *curve = profile_curve(profile);
    int32_t tau_ms = target >= current ? curve->rise_tau_ms : curve->fall_tau_ms;
    int32_t alpha_q16 = (int32_t)((int64_t)dt_ms * Q16_ONE / (tau_ms + dt_ms));
    int32_t next = current + (int32_t)((int64_t)(target - current) * alpha_q16 / Q16_ONE);
    return CLAMP(next, curve->points[0].gain_q16,
                 curve->points[curve->point_count - 1].gain_q16);
}

static uint32_t physical_speed_mm_s(int64_t dx, int64_t dy, uint16_t cpi, int32_t dt_ms) {
    float x = (float)dx;
    float y = (float)dy;
    float magnitude = sqrtf(x * x + y * y);
    float speed = magnitude * 25400.0f /
                  ((float)(cpi > 0 ? cpi : 1) * (float)(dt_ms > 0 ? dt_ms : 1));

    if (speed <= 0.0f) {
        return 0;
    }
    if (!isfinite(speed) || speed >= (float)UINT32_MAX) {
        return UINT32_MAX;
    }

    uint64_t rounded = (uint64_t)(speed + 0.5f);
    return rounded > UINT32_MAX ? UINT32_MAX : (uint32_t)rounded;
}

static bool direction_changed(const struct meteorite_motion_scaler_data *data, int64_t frame_x,
                              int64_t frame_y) {
    if ((data->previous_frame_x == 0 && data->previous_frame_y == 0) ||
        (frame_x == 0 && frame_y == 0)) {
        return false;
    }

    /* Normalize before the dot product so even synthetic INT64 boundary input
     * cannot overflow. dot <= 0 means a turn of at least 90 degrees. */
    float previous_scale =
        MAX(fabsf((float)data->previous_frame_x), fabsf((float)data->previous_frame_y));
    float frame_scale = MAX(fabsf((float)frame_x), fabsf((float)frame_y));
    float dot = ((float)data->previous_frame_x / previous_scale) *
                    ((float)frame_x / frame_scale) +
                ((float)data->previous_frame_y / previous_scale) *
                    ((float)frame_y / frame_scale);
    return dot <= 0.0f;
}

static int64_t saturating_add_delta(int64_t value, int32_t delta) {
    if (delta > 0 && value > INT64_MAX - delta) {
        return INT64_MAX;
    }
    if (delta < 0 && value < INT64_MIN - delta) {
        return INT64_MIN;
    }
    return value + delta;
}

static int32_t apply_gain_axis_q16(int32_t input, int32_t gain_q16, int32_t *remainder_q16) {
    if (input == 0) {
        return 0;
    }

    int64_t scaled_q16 = (int64_t)input * gain_q16 + *remainder_q16;
    int64_t output = scaled_q16 / Q16_ONE;
    if (output > INT32_MAX) {
        *remainder_q16 = 0;
        return INT32_MAX;
    }
    if (output < INT32_MIN) {
        *remainder_q16 = 0;
        return INT32_MIN;
    }

    *remainder_q16 = (int32_t)(scaled_q16 - output * Q16_ONE);
    return (int32_t)output;
}

static void current_config(const struct meteorite_motion_scaler_config *config, bool *enabled,
                           uint8_t *profile, uint16_t *cpi) {
    *enabled = config->scaling_mode != 0;
    *profile = sanitize_profile(config->pointer_profile);
    *cpi = (uint16_t)CLAMP(config->cpi, 1, UINT16_MAX);

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    *enabled = zmk_custom_config_scaling_enabled();
    *profile = sanitize_profile(zmk_custom_config_pointer_profile());
    uint16_t custom_cpi = zmk_custom_config_cpi_value();
    *cpi = custom_cpi > 0 ? custom_cpi : 1;
#endif
}

static bool config_changed(const struct meteorite_motion_scaler_data *data, bool enabled,
                           uint8_t profile, uint16_t cpi) {
    return !data->config_initialized || data->last_enabled != enabled ||
           data->last_profile != profile || data->last_cpi != cpi;
}

static void remember_config(struct meteorite_motion_scaler_data *data, bool enabled,
                            uint8_t profile, uint16_t cpi) {
    data->last_enabled = enabled;
    data->last_profile = profile;
    data->last_cpi = cpi;
    data->config_initialized = true;
}

static void scale_rel_axis(struct meteorite_motion_scaler_data *data, struct input_event *event,
                           int64_t *frame, int32_t *remainder_q16, const char *tag) {
    int32_t input = event->value;
    *frame = saturating_add_delta(*frame, input);
    event->value = apply_gain_axis_q16(input, data->gain_q16, remainder_q16);
    LOG_DBG("motion %s in=%d out=%d rem_q16=%d gain_q16=%d", tag, input, event->value,
            *remainder_q16, data->gain_q16);
}

static void reset_for_direction_change(struct meteorite_motion_scaler_data *data,
                                       atomic_val_t generation, bool enabled, uint8_t profile,
                                       uint16_t cpi, int64_t prospective_x,
                                       int64_t prospective_y) {
    if (!direction_changed(data, prospective_x, prospective_y)) {
        return;
    }

    /* Preserve raw deltas already seen in this frame for the sync-time speed
     * calculation. Their output has already left this processor, but gain and
     * remainder state must still be dropped before the event that reveals the
     * turn is scaled. */
    int64_t partial_x = data->frame_x;
    int64_t partial_y = data->frame_y;
    reset_motion_state(data, generation, profile);
    remember_config(data, enabled, profile, cpi);
    data->frame_x = partial_x;
    data->frame_y = partial_y;
    LOG_DBG("motion direction reset profile=%u", profile);
}

static int meteorite_motion_scaler_handle_event(const struct device *dev,
                                                 struct input_event *event, uint32_t param1,
                                                 uint32_t param2,
                                                 struct zmk_input_processor_state *state) {
    struct meteorite_motion_scaler_data *data = dev->data;
    const struct meteorite_motion_scaler_config *config = dev->config;

    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    bool enabled;
    uint8_t profile;
    uint16_t cpi;
    current_config(config, &enabled, &profile, &cpi);

    atomic_val_t generation = atomic_get(&motion_reset_generation);
    int64_t now = k_uptime_get();
    bool stopped = data->last_frame_ms > 0 && now - data->last_frame_ms > MOTION_STOP_RESET_MS;
    if (generation != data->reset_generation || config_changed(data, enabled, profile, cpi) ||
        stopped) {
        reset_motion_state(data, generation, profile);
        remember_config(data, enabled, profile, cpi);
    }

    if (!enabled) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type == INPUT_EV_REL) {
        if (event->code == INPUT_REL_X) {
            /* A >=90 degree turn can only be classified from the complete
             * frame. Checking a partial X-only vector here would make the
             * result depend on whether X or Y happened to arrive first. */
            if (event->sync) {
                int64_t prospective_x = saturating_add_delta(data->frame_x, event->value);
                reset_for_direction_change(data, generation, enabled, profile, cpi,
                                           prospective_x, data->frame_y);
            }
            scale_rel_axis(data, event, &data->frame_x, &data->remainder_x_q16, "REL_X");
        } else if (event->code == INPUT_REL_Y) {
            if (event->sync) {
                int64_t prospective_y = saturating_add_delta(data->frame_y, event->value);
                reset_for_direction_change(data, generation, enabled, profile, cpi, data->frame_x,
                                           prospective_y);
            }
            scale_rel_axis(data, event, &data->frame_y, &data->remainder_y_q16, "REL_Y");
        }
    }

    /* The full vector is known only at sync, so the resulting smoothed gain is
     * deliberately applied to the next frame. */
    if (!event->sync) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    /* Some input sources emit sync separately from the final axis event. In
     * that case both axes have already been scaled, but reset the completed
     * vector here so stale gain/remainder never crosses into the next frame. */
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        reset_for_direction_change(data, generation, enabled, profile, cpi, data->frame_x,
                                   data->frame_y);
    }

    int64_t frame_x = data->frame_x;
    int64_t frame_y = data->frame_y;
    data->frame_x = 0;
    data->frame_y = 0;
    if (frame_x == 0 && frame_y == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int32_t dt_ms = config->default_dt_ms > 0 ? config->default_dt_ms : 12;
    if (data->last_frame_ms > 0) {
        dt_ms = (int32_t)(now - data->last_frame_ms);
    }
    dt_ms = CLAMP(dt_ms, MOTION_DT_MIN_MS, MOTION_DT_MAX_MS);

    uint32_t speed_mm_s = physical_speed_mm_s(frame_x, frame_y, cpi, dt_ms);
    int32_t target_gain_q16 = gain_for_speed_q16(profile, speed_mm_s);
    data->gain_q16 = smooth_gain_q16(profile, data->gain_q16, target_gain_q16, dt_ms);
    data->previous_frame_x = frame_x;
    data->previous_frame_y = frame_y;
    data->last_frame_ms = now;

    LOG_DBG("motion profile=%u speed=%u dt=%d gain_q16=%d target_q16=%d frame=(%lld,%lld)",
            profile, speed_mm_s, dt_ms, data->gain_q16, target_gain_q16,
            (long long)frame_x, (long long)frame_y);
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api meteorite_motion_scaler_driver_api = {
    .handle_event = meteorite_motion_scaler_handle_event,
};

#define METEORITE_MOTION_SCALER_INST(n)                                                           \
    static struct meteorite_motion_scaler_data meteorite_motion_scaler_data_##n = {                \
        .gain_q16 = Q16_ONE,                                                                       \
    };                                                                                             \
    static const struct meteorite_motion_scaler_config meteorite_motion_scaler_config_##n = {      \
        .scaling_mode = DT_INST_PROP(n, scaling_mode),                                             \
        .pointer_profile = DT_INST_PROP(n, pointer_profile),                                       \
        .cpi = DT_INST_PROP(n, cpi),                                                               \
        .default_dt_ms = DT_INST_PROP(n, default_dt_ms),                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_motion_scaler_data_##n,                        \
                          &meteorite_motion_scaler_config_##n, POST_KERNEL,                        \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                          &meteorite_motion_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_MOTION_SCALER_INST)
