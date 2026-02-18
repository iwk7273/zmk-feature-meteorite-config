/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/sys/poweroff.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(meteorite_idle_poweroff, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

static int meteorite_idle_poweroff_listener(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (!ev) {
        return 0;
    }

    if (ev->state == ZMK_ACTIVITY_IDLE) {
        LOG_INF("IDLE -> poweroff (ignore USB power)");
        sys_poweroff();
    }

    return 0;
}

ZMK_LISTENER(meteorite_idle_poweroff, meteorite_idle_poweroff_listener);
ZMK_SUBSCRIPTION(meteorite_idle_poweroff, zmk_activity_state_changed);
