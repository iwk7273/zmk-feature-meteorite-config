#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/custom_config_axes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of keymap layers a ball profile can be assigned to. Must match
 * the meteorite.proto BallConfig.layer_profiles max_count. */
#define ZMK_CUSTOM_CONFIG_MAX_LAYERS 16
/* Number of ball directions (LEFT, RIGHT, UP, DOWN). */
#define ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS 4

#define ZMK_CUSTOM_CONFIG_TAPPING_TERM_MIN_MS 50
#define ZMK_CUSTOM_CONFIG_TAPPING_TERM_MAX_MS 500
#define ZMK_CUSTOM_CONFIG_TAPPING_TERM_STEP_MS 10
#define ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MIN_MS 10
#define ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_MAX_MS 500
#define ZMK_CUSTOM_CONFIG_HOLD_TAP_TIMING_STEP_MS 10
#define ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_MIN_S 30
#define ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_MAX_S 1800
#define ZMK_CUSTOM_CONFIG_IDLE_TIMEOUT_STEP_S 30
#define ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_MIN_S 60
#define ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_MAX_S 7200
#define ZMK_CUSTOM_CONFIG_IDLE_SLEEP_TIMEOUT_STEP_S 60

/* Ball profile assigned to a layer. Values must match zmk.meteorite.BallProfile. */
enum zmk_ball_profile {
    ZMK_BALL_PROFILE_OFF = 0,
    ZMK_BALL_PROFILE_SCROLL = 1,
    ZMK_BALL_PROFILE_BROWSER = 2,
    ZMK_BALL_PROFILE_DESKTOP = 3,
    ZMK_BALL_PROFILE_WINDOW = 4,
    ZMK_BALL_PROFILE_APP = 5,
    ZMK_BALL_PROFILE_USER1 = 6,
    ZMK_BALL_PROFILE_COUNT,
};

/* Stored scroll response. Values 0/1 intentionally preserve the legacy
 * scaling OFF/ON wire and NVS representation. */
enum zmk_scroll_scaling_mode {
    ZMK_SCROLL_SCALING_MODE_LINEAR = 0,
    ZMK_SCROLL_SCALING_MODE_ADAPTIVE = 1,
    ZMK_SCROLL_SCALING_MODE_COUNT,
};

/* Pointer response profile. Values must match zmk.meteorite.PointerProfile. */
enum zmk_pointer_profile {
    ZMK_POINTER_PROFILE_STANDARD = 0,
    ZMK_POINTER_PROFILE_STABLE = 1,
    ZMK_POINTER_PROFILE_RESPONSIVE = 2,
    ZMK_POINTER_PROFILE_CUSTOM = 3,
    ZMK_POINTER_PROFILE_COUNT,
};

enum zmk_pointer_curve_point {
    ZMK_POINTER_CURVE_POINT_START = 0,
    ZMK_POINTER_CURVE_POINT_PRECISION = 1,
    ZMK_POINTER_CURVE_POINT_FAST = 2,
    ZMK_POINTER_CURVE_POINT_FLICK = 3,
    ZMK_POINTER_CURVE_POINT_COUNT,
};

/* Shared action-profile sensitivity. Values must match zmk.meteorite.BallSensitivity.
 * The integer values are in sensitivity order (VERY_LIGHT most sensitive ..
 * VERY_HEAVY least), so a slider position maps directly to the value. Thresholds
 * are a value-indexed lookup (see state.c). Any future level must be appended at
 * the end (>= 5) to keep saved configs stable; do not renumber these. */
enum zmk_ball_sensitivity {
    ZMK_BALL_SENSITIVITY_VERY_LIGHT = 0,
    ZMK_BALL_SENSITIVITY_LIGHT = 1,
    ZMK_BALL_SENSITIVITY_NORMAL = 2,
    ZMK_BALL_SENSITIVITY_HEAVY = 3,
    ZMK_BALL_SENSITIVITY_VERY_HEAVY = 4,
    ZMK_BALL_SENSITIVITY_COUNT,
};

/* Ball direction index into a profile's bindings. Values must match
 * zmk.meteorite.BallDirection. */
enum zmk_ball_direction {
    ZMK_BALL_DIR_LEFT = 0,
    ZMK_BALL_DIR_RIGHT = 1,
    ZMK_BALL_DIR_UP = 2,
    ZMK_BALL_DIR_DOWN = 3,
    ZMK_BALL_DIR_COUNT,
};

/* Values must match zmk.meteorite.HoldTapFlavor and ZMK's hold-tap flavor
 * ordering. Keep existing values stable because they are persisted in NVS. */
enum zmk_custom_config_hold_tap_flavor {
    ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_HOLD_PREFERRED = 0,
    ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_BALANCED = 1,
    ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_TAP_PREFERRED = 2,
    ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_TAP_UNLESS_INTERRUPTED = 3,
    ZMK_CUSTOM_CONFIG_HOLD_TAP_FLAVOR_COUNT,
};

/* A user-defined (USER1) binding for one direction. behavior_local_id is the
 * ZMK Studio behavior id (local id); it is resolved to a behavior name at fire
 * time. local_id 0 means "no binding" (no-op). */
struct zmk_custom_config_ball_binding {
    uint16_t behavior_local_id;
    uint32_t param1;
    uint32_t param2;
};

struct zmk_custom_config {
    uint8_t cpi_idx;
    uint8_t scroll_div;
    uint8_t rotation_idx;
    uint8_t scroll_h_rev;
    uint8_t scroll_v_rev;
    uint8_t scaling_mode;
    uint8_t scroll_scaling_mode;
    /* Frozen legacy scalars. No longer used for routing; kept at defaults for
     * backward compatibility. Superseded by layer_profiles. */
    uint8_t scroll_layer_1;
    uint8_t scroll_layer_2;
    uint8_t os_mode;
    /* Ball profile state (settings schema v4+). */
    uint8_t ball_sensitivity;
    uint8_t layer_profiles[ZMK_CUSTOM_CONFIG_MAX_LAYERS];
    struct zmk_custom_config_ball_binding user1[ZMK_CUSTOM_CONFIG_BALL_DIRECTIONS];
    /* Keyboard behavior and power-management settings. Power values are in
     * seconds; 0 disables the corresponding automatic state transition. */
    uint16_t mod_tap_tapping_term_ms;
    uint16_t layer_tap_tapping_term_ms;
    uint16_t idle_timeout_s;
    uint16_t idle_sleep_timeout_s;
    uint8_t mod_tap_flavor;
    uint16_t mod_tap_quick_tap_ms;
    uint16_t mod_tap_require_prior_idle_ms;
    uint8_t layer_tap_flavor;
    uint16_t layer_tap_quick_tap_ms;
    uint16_t layer_tap_require_prior_idle_ms;
    /* Pointer response profile. Appended to keep runtime evolution explicit. */
    uint8_t pointer_profile;
    /* Percent gains at fixed 0 / 30 / 90 / 200 mm/s custom-curve points. */
    uint16_t pointer_custom_gain_percent[ZMK_POINTER_CURVE_POINT_COUNT];
};

const struct zmk_custom_config *zmk_custom_config_get(void);
const struct zmk_custom_config *zmk_custom_config_saved_get(void);
const struct zmk_custom_config *zmk_custom_config_defaults_get(void);
int zmk_custom_config_set(const struct zmk_custom_config *cfg);
int zmk_custom_config_apply_op(uint8_t op);
int zmk_custom_config_save(void);
int zmk_custom_config_discard(void);
int zmk_custom_config_reset_settings(void);
bool zmk_custom_config_check_unsaved_changes(void);

uint16_t zmk_custom_config_cpi_value(void);
uint16_t zmk_custom_config_scroll_div_value(void);
int16_t zmk_custom_config_rotation_deg(void);
uint8_t zmk_custom_config_cpi_count(void);
uint8_t zmk_custom_config_scroll_div_count(void);
uint8_t zmk_custom_config_rotation_count(void);
uint8_t zmk_custom_config_layer_count(void);
int16_t zmk_custom_config_rotation_deg_at(uint8_t index);
bool zmk_custom_config_scroll_h_rev(void);
bool zmk_custom_config_scroll_v_rev(void);
bool zmk_custom_config_scaling_enabled(void);
uint8_t zmk_custom_config_pointer_profile(void);
uint16_t zmk_custom_config_pointer_gain_percent(uint8_t point);
uint16_t zmk_custom_config_pointer_curve_speed_mm_s(uint8_t point);
uint8_t zmk_custom_config_pointer_gain_option_count(uint8_t point);
uint16_t zmk_custom_config_pointer_gain_option_at(uint8_t point, uint8_t option);
bool zmk_custom_config_pointer_curve_is_valid(const struct zmk_custom_config *cfg);
uint8_t zmk_custom_config_scroll_scaling_mode(void);
/* Compatibility accessor for older callers. Prefer the mode getter above. */
bool zmk_custom_config_scroll_scaling_enabled(void);
uint8_t zmk_custom_config_scroll_layer_1(void);
uint8_t zmk_custom_config_scroll_layer_2(void);
bool zmk_custom_config_os_is_mac(void);
bool zmk_custom_config_is_ready(void);
uint16_t zmk_custom_config_mod_tap_tapping_term_ms(void);
uint16_t zmk_custom_config_layer_tap_tapping_term_ms(void);
uint8_t zmk_custom_config_mod_tap_flavor(void);
uint16_t zmk_custom_config_mod_tap_quick_tap_ms(void);
uint16_t zmk_custom_config_mod_tap_require_prior_idle_ms(void);
uint8_t zmk_custom_config_layer_tap_flavor(void);
uint16_t zmk_custom_config_layer_tap_quick_tap_ms(void);
uint16_t zmk_custom_config_layer_tap_require_prior_idle_ms(void);
uint16_t zmk_custom_config_idle_timeout_s(void);
uint16_t zmk_custom_config_idle_sleep_timeout_s(void);

/* Ball profile accessors. */
/* Profile assigned to a given layer index (clamped/sanitized). */
uint8_t zmk_custom_config_layer_profile(uint8_t layer_index);
/* Profile of the highest currently-active layer (the effective profile). */
uint8_t zmk_custom_config_active_profile(void);
/* Shared action-profile sensitivity (enum zmk_ball_sensitivity). */
uint8_t zmk_custom_config_ball_sensitivity(void);
/* Accumulation threshold in sensor counts for the current sensitivity. */
uint16_t zmk_custom_config_ball_threshold(void);
/* Minimum spacing (ms) between fired action taps for the current sensitivity. */
uint16_t zmk_custom_config_ball_cooldown_ms(void);
/* USER1 binding for a direction (enum zmk_ball_direction); NULL if out of range. */
const struct zmk_custom_config_ball_binding *zmk_custom_config_user1_binding(uint8_t direction);

/* Optional hook to react to state changes from settings or toggles. */
void zmk_custom_config_changed(const struct zmk_custom_config *cfg);

#ifdef __cplusplus
}
#endif
