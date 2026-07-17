#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/custom_config_axes.h>
#include <zmk/custom_feature.h>

#define CUSTOM_CPI_DEFAULT 4
#define CUSTOM_CPI_MAX ZMK_CUSTOM_CONFIG_CPI_MAX
#define CUSTOM_CPI_STEP ZMK_CUSTOM_CONFIG_CPI_STEP
#define CUSTOM_SCROLL_DIV_DEFAULT 3
#define CUSTOM_SCROLL_DIV_MAX ZMK_CUSTOM_CONFIG_SCROLL_DIV_MAX
#define CUSTOM_ROTATION_DEFAULT 20
#define CUSTOM_ROTATION_ANGLE_COUNT 29

extern const int16_t zmk_custom_config_rotation_angles[CUSTOM_ROTATION_ANGLE_COUNT];

static inline uint16_t zmk_custom_config_cpi_value_for(const struct zmk_custom_config *cfg) {
    return zmk_custom_config_axis_idx_to_value(zmk_custom_config_cpi_axis(), cfg->cpi_idx);
}

static inline uint16_t
zmk_custom_config_scroll_div_value_for(const struct zmk_custom_config *cfg) {
    return zmk_custom_config_axis_idx_to_value(zmk_custom_config_scroll_div_axis(),
                                              cfg->scroll_div);
}

void zmk_custom_config_log(const char *tag, const struct zmk_custom_config *cfg);

void zmk_custom_config_set_defaults(struct zmk_custom_config *cfg);
void zmk_custom_config_sanitize_layers(struct zmk_custom_config *cfg);
void zmk_custom_config_sanitize_scroll_scaling(struct zmk_custom_config *cfg);
void zmk_custom_config_sanitize_timing(struct zmk_custom_config *cfg);
/* Clamp ball profile/sensitivity values to valid ranges. USER1 bindings whose
 * local id cannot be resolved are left in place and treated as no-op at fire
 * time (resolution is lazy in the router). */
void zmk_custom_config_sanitize_ball(struct zmk_custom_config *cfg);

void zmk_custom_config_apply_cpi(const struct zmk_custom_config *cfg);

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg);
int zmk_custom_config_storage_delete(void);
void zmk_custom_config_handle_loaded_settings(struct zmk_custom_config *cfg);
void zmk_custom_config_commit_settings(bool settings_loaded);

int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag);
int zmk_custom_config_schedule_os_mode_save(void);
