#define DT_DRV_COMPAT zmk_input_processor_meteorite_motion_scaler

#include <drivers/input_processor.h>
#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
#include <zmk/custom_feature.h>
#endif

LOG_MODULE_REGISTER(meteorite_motion_scaler, CONFIG_ZMK_LOG_LEVEL);

#define Q16_ONE (1 << 16)

/* Upper bound for the Q16 gain before lrintf(): kept safely below INT32_MAX so
 * the float -> int32_t conversion can never overflow. Defined as a float because
 * it is only ever compared/clamped in floating-point math. */
#define GAIN_Q16_LRINTF_SAFE_MAX 2147483000.0f

/*
 * Pointer / scroll acceleration.
 *
 * Per input frame (the run of REL events up to and including event->sync) the
 * X/Y deltas are summed into (acc_x, acc_y). At frame end the magnitude
 *     mag = hypot(acc_x, acc_y)
 * is mapped through a saturating curve
 *     ymag = max_output * r^e / (1 + r^e),  r = mag / half_input,  e = 1 + exponent_tenths / 10
 * and the per-frame gain k = ymag / mag is stored (Q16.16). That gain is applied
 * to each axis of the *next* frame: there is a deliberate one-frame lag because
 * the full frame magnitude is not known until sync arrives. With track-remainders
 * the Q16 fractional output is carried into the next event so sub-unit motion is
 * not lost to truncation.
 *
 * NOTE: unlike the sibling processors (xy_clipper / sensor_rotation /
 * ball_profile) this one must NOT early-return on a non-REL event type. The sync
 * event closes the frame and recomputes the gain, so event->sync is handled
 * outside the INPUT_EV_REL guard below; an early type guard would hide it.
 */

struct meteorite_motion_scaler_data {
    int32_t remainder_x_q16;
    int32_t remainder_y_q16;
    int32_t gain_q16;
    int32_t acc_x;
    int32_t acc_y;
};

struct meteorite_motion_scaler_config {
    /* When CONFIG_ZMK_CUSTOM_CONFIG is enabled, selects which custom-config
     * toggle gates this instance: 0 = cursor scaling (custom-config-scaling = <0>,
     * reads zmk_custom_config_scaling_enabled), 1 = scroll scaling
     * (custom-config-scaling = <1>, reads zmk_custom_config_scroll_scaling_enabled). */
    int32_t custom_config_scaling;
    /* Fallback enable used only when CONFIG_ZMK_CUSTOM_CONFIG is disabled:
     * 0 = scaling off, non-zero = on. */
    int32_t scaling_mode;
    int max_output;
    int half_input;
    int exponent_tenths;
    bool track_remainders;
};

static inline float scale_magnitude(float mag,
                                    const struct meteorite_motion_scaler_config *config) {
    if (mag <= 0.0f) {
        return 0.0f;
    }

    const int xs = (config->half_input > 0) ? config->half_input : 1;
    const float r = mag / (float)xs;
    float p = (float)((config->exponent_tenths < 0) ? 0 : config->exponent_tenths) / 10.0f;
    const float e = p + 1.0f;
    float rp1 = powf(r, e);

    if (!isfinite(rp1)) {
        rp1 = INFINITY;
    }

    const float frac = rp1 / (1.0f + rp1);
    float ymag = (float)config->max_output * frac;

    if (!isfinite(ymag)) {
        ymag = (float)config->max_output;
    }

    if (ymag < 0.0f) {
        ymag = 0.0f;
    }

    return ymag;
}

static inline int32_t clamp_axis_output(int32_t v, int max_output) {
    return CLAMP(v, -max_output, max_output);
}

static inline int32_t apply_gain_axis_q16(int32_t in, int32_t gain_q16,
                                          const struct meteorite_motion_scaler_config *config,
                                          int32_t *remainder_q16) {
    if (in == 0) {
        return 0;
    }

    int64_t scaled_q16 = (int64_t)in * (int64_t)gain_q16;
    if (config->track_remainders) {
        scaled_q16 += *remainder_q16;
    }

    int32_t out = (int32_t)(scaled_q16 / Q16_ONE);
    if (config->track_remainders) {
        *remainder_q16 = (int32_t)(scaled_q16 - (int64_t)out * Q16_ONE);
    } else {
        *remainder_q16 = 0;
    }

    return clamp_axis_output(out, config->max_output);
}

static inline bool scaler_enabled(const struct meteorite_motion_scaler_config *config) {
#if IS_ENABLED(CONFIG_ZMK_CUSTOM_CONFIG)
    return config->custom_config_scaling ? zmk_custom_config_scroll_scaling_enabled()
                                         : zmk_custom_config_scaling_enabled();
#else
    return config->scaling_mode != 0;
#endif
}

/* Accumulate the raw delta for this frame, apply the previous frame's gain to the
 * event (updating the Q16 remainder), and write the scaled value back. */
static inline void scale_rel_axis(struct meteorite_motion_scaler_data *data,
                                  const struct meteorite_motion_scaler_config *config,
                                  struct input_event *event, int32_t *acc, int32_t *remainder_q16,
                                  const char *tag) {
    int32_t in = event->value;
    *acc += in;
    int32_t out = apply_gain_axis_q16(in, data->gain_q16, config, remainder_q16);
    event->value = out;
    LOG_DBG("meteorite_motion_scaler %s in=%d out=%d rem_q16=%d k_q16=%d", tag, in, out,
            *remainder_q16, data->gain_q16);
}

static inline int32_t
compute_next_gain_q16_from_acc(const struct meteorite_motion_scaler_data *data,
                               const struct meteorite_motion_scaler_config *config) {
    float ax = (float)data->acc_x;
    float ay = (float)data->acc_y;
    float mag = sqrtf(ax * ax + ay * ay);

    if (mag <= 0.0f) {
        return Q16_ONE;
    }

    float kf = scale_magnitude(mag, config) / mag;
    if (!isfinite(kf) || kf < 0.0f) {
        return 0;
    }

    /* kf is finite and >= 0 here, so only the upper bound needs clamping. */
    return (int32_t)lrintf(MIN(kf * (float)Q16_ONE, GAIN_Q16_LRINTF_SAFE_MAX));
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

    if (!scaler_enabled(config)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type == INPUT_EV_REL) {
        if (event->code == INPUT_REL_X) {
            scale_rel_axis(data, config, event, &data->acc_x, &data->remainder_x_q16, "REL_X");
        } else if (event->code == INPUT_REL_Y) {
            scale_rel_axis(data, config, event, &data->acc_y, &data->remainder_y_q16, "REL_Y");
        }
    }

    /* event->sync is handled outside the INPUT_EV_REL guard on purpose: it ends
     * the frame and recomputes the gain for the next one (see module header). */
    if (event->sync) {
        data->gain_q16 = compute_next_gain_q16_from_acc(data, config);
        data->acc_x = 0;
        data->acc_y = 0;
        LOG_DBG("meteorite_motion_scaler frame end: k_q16=%d", data->gain_q16);
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api meteorite_motion_scaler_driver_api = {
    .handle_event = meteorite_motion_scaler_handle_event,
};

#define METEORITE_MOTION_SCALER_INST(n)                                                           \
    static struct meteorite_motion_scaler_data meteorite_motion_scaler_data_##n = {                \
        .remainder_x_q16 = 0,                                                                      \
        .remainder_y_q16 = 0,                                                                      \
        .gain_q16 = Q16_ONE,                                                                       \
        .acc_x = 0,                                                                                \
        .acc_y = 0,                                                                                \
    };                                                                                             \
    static const struct meteorite_motion_scaler_config meteorite_motion_scaler_config_##n = {      \
        .custom_config_scaling = DT_INST_PROP_OR(n, custom_config_scaling, 0),                     \
        .scaling_mode = DT_INST_PROP_OR(n, scaling_mode, 0),                                       \
        .max_output = DT_INST_PROP_OR(n, max_output, 127),                                         \
        .half_input = DT_INST_PROP_OR(n, half_input, 50),                                          \
        .exponent_tenths = DT_INST_PROP_OR(n, exponent_tenths, 10),                                \
        .track_remainders = DT_INST_PROP(n, track_remainders),                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &meteorite_motion_scaler_data_##n,                        \
                          &meteorite_motion_scaler_config_##n, POST_KERNEL,                        \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &meteorite_motion_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(METEORITE_MOTION_SCALER_INST)
