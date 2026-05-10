/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CUSTOM_CONFIG_SETTINGS_KEY "custom_config/state"

#if IS_ENABLED(CONFIG_SETTINGS)
static bool settings_init;

int zmk_custom_config_storage_save(const struct zmk_custom_config *cfg) {
    int ret = settings_save_one(CUSTOM_CONFIG_SETTINGS_KEY, cfg, sizeof(*cfg));
    if (ret < 0) {
        LOG_WRN("Failed to save custom config (%d)", ret);
    } else {
        LOG_INF("Saved custom config");
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

    struct zmk_custom_config loaded;
    const size_t size_v2 = sizeof(loaded);
    const size_t size_v2_no_os = size_v2 - 1;
    const size_t size_v2_no_scroll_scaling = size_v2 - 2;

    if (len != size_v2 && len != size_v2_no_os && len != size_v2_no_scroll_scaling) {
        return -EINVAL;
    }

    memset(&loaded, 0, sizeof(loaded));

    int rc = read_cb(cb_arg, &loaded, len);
    if (rc < 0) {
        return rc;
    }

    if (len != size_v2) {
        struct zmk_custom_config defaults;
        zmk_custom_config_set_defaults(&defaults);

        if (len <= size_v2_no_scroll_scaling) {
            loaded.scroll_scaling_mode = defaults.scroll_scaling_mode;
        }
        loaded.os_mode = defaults.os_mode;
    }

    zmk_custom_config_handle_loaded_settings(&loaded);
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
