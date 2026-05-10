/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CUSTOM_CONFIG_SETTINGS_KEY "custom_config/state"

#if IS_ENABLED(CONFIG_SETTINGS)

#define CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT 3U
#define CUSTOM_CONFIG_SCHEMA_VERSION_MARKER 0x80U
#define CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(version)                                                \
    ((uint8_t)(CUSTOM_CONFIG_SCHEMA_VERSION_MARKER | (version)))
#define CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT_BYTE                                                 \
    CUSTOM_CONFIG_SCHEMA_VERSION_BYTE(CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT)

struct custom_config_settings_current {
    uint8_t schema_version;
    struct zmk_custom_config payload;
};

BUILD_ASSERT(sizeof(struct zmk_custom_config) == 10,
             "custom config layout changed; bump settings schema version");
BUILD_ASSERT(sizeof(struct custom_config_settings_current) == 11,
             "unexpected custom config settings size");

static bool settings_init;

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    struct custom_config_settings_current stored = {
        .schema_version = CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT_BYTE,
        .payload = *cfg,
    };

    int ret = settings_save_one(CUSTOM_CONFIG_SETTINGS_KEY, &stored, sizeof(stored));
    if (ret < 0) {
        LOG_WRN("Failed to save custom config (%d)", ret);
    } else {
        LOG_INF("Saved custom config schema=v%u", CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT);
    }
    return ret;
}

int zmk_custom_config_storage_delete(void) {
    int ret = settings_delete(CUSTOM_CONFIG_SETTINGS_KEY);
    if (ret < 0) {
        LOG_WRN("Failed to delete custom config settings (%d)", ret);
    } else {
        LOG_INF("Deleted custom config settings");
    }
    return ret;
}

static int custom_feature_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                       void *cb_arg) {
    if (!settings_name_steq(name, "state", NULL)) {
        return -ENOENT;
    }

    if (len != sizeof(struct custom_config_settings_current)) {
        LOG_WRN("Ignoring custom config settings with incompatible size %zu", len);
        return 0;
    }

    struct custom_config_settings_current stored;

    int rc = read_cb(cb_arg, &stored, sizeof(stored));
    if (rc < 0) {
        return rc;
    }
    if ((size_t)rc != sizeof(stored)) {
        return -EINVAL;
    }

    if (stored.schema_version != CUSTOM_CONFIG_SCHEMA_VERSION_CURRENT_BYTE) {
        LOG_WRN("Ignoring custom config settings schema 0x%02x", stored.schema_version);
        return 0;
    }

    zmk_custom_config_handle_loaded_settings(&stored.payload);
    settings_init = true;
    return 0;
}

static int custom_feature_settings_commit(void) {
    zmk_custom_config_commit_settings(settings_init);
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(custom_feature, "custom_config", NULL,
                               custom_feature_settings_set, custom_feature_settings_commit, NULL);
#else
int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    ARG_UNUSED(cfg);
    return 0;
}

int zmk_custom_config_storage_delete(void) { return 0; }
#endif
