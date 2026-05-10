#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/custom_feature.h>

#define CUSTOM_CPI_DEFAULT 4
#define CUSTOM_CPI_MAX 16
#define CUSTOM_CPI_STEP 200
#define CUSTOM_SCROLL_DIV_DEFAULT 3
#define CUSTOM_SCROLL_DIV_MAX 16
#define CUSTOM_ROTATION_DEFAULT 20
#define CUSTOM_ROTATION_ANGLE_COUNT 29

extern const int16_t zmk_custom_config_rotation_angles[CUSTOM_ROTATION_ANGLE_COUNT];

static inline uint16_t zmk_custom_config_cpi_value_for(const struct zmk_custom_config *cfg) {
    return (cfg->cpi_idx + 1) * CUSTOM_CPI_STEP;
}

static inline uint16_t
zmk_custom_config_scroll_div_value_for(const struct zmk_custom_config *cfg) {
    return (cfg->scroll_div + 1) * 5;
}

void zmk_custom_config_log(const char *tag, const struct zmk_custom_config *cfg);

void zmk_custom_config_set_defaults(struct zmk_custom_config *cfg);
void zmk_custom_config_sanitize_layers(struct zmk_custom_config *cfg);

void zmk_custom_config_apply_cpi(const struct zmk_custom_config *cfg);

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg);
int zmk_custom_config_storage_delete(void);
void zmk_custom_config_handle_loaded_settings(struct zmk_custom_config *cfg);
void zmk_custom_config_commit_settings(bool settings_loaded);

int zmk_custom_config_set_with_tag(const struct zmk_custom_config *cfg, const char *tag);
