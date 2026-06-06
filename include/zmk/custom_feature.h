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

/* Shared action-profile sensitivity. Values must match zmk.meteorite.BallSensitivity.
 * The integer values are intentionally NOT in sensitivity order: LIGHT/NORMAL/
 * HEAVY keep their original 0/1/2 (so saved configs are never reinterpreted) and
 * the two extremes are appended as 3/4. Sensitivity-ordered display sequence is
 * VERY_LIGHT, LIGHT, NORMAL, HEAVY, VERY_HEAVY. Thresholds are a value-indexed
 * lookup (see state.c), so the out-of-order values do not matter at fire time. */
enum zmk_ball_sensitivity {
    ZMK_BALL_SENSITIVITY_LIGHT = 0,
    ZMK_BALL_SENSITIVITY_NORMAL = 1,
    ZMK_BALL_SENSITIVITY_HEAVY = 2,
    ZMK_BALL_SENSITIVITY_VERY_LIGHT = 3,
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
bool zmk_custom_config_scroll_scaling_enabled(void);
uint8_t zmk_custom_config_scroll_layer_1(void);
uint8_t zmk_custom_config_scroll_layer_2(void);
bool zmk_custom_config_os_is_mac(void);

/* Ball profile accessors. */
/* Profile assigned to a given layer index (clamped/sanitized). */
uint8_t zmk_custom_config_layer_profile(uint8_t layer_index);
/* Profile of the highest currently-active layer (the effective profile). */
uint8_t zmk_custom_config_active_profile(void);
/* Shared action-profile sensitivity (enum zmk_ball_sensitivity). */
uint8_t zmk_custom_config_ball_sensitivity(void);
/* Accumulation threshold in sensor counts for the current sensitivity. */
uint16_t zmk_custom_config_ball_threshold(void);
/* USER1 binding for a direction (enum zmk_ball_direction); NULL if out of range. */
const struct zmk_custom_config_ball_binding *zmk_custom_config_user1_binding(uint8_t direction);

/* Optional hook to react to state changes from settings or toggles. */
void zmk_custom_config_changed(const struct zmk_custom_config *cfg);

#ifdef __cplusplus
}
#endif
