#pragma once

#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Drop pointer gain, frame, and Q16 remainder state on the next input event.
 * Ball-profile routing and custom-config transitions use this so no motion is
 * carried across a semantic mode change. */
#if IS_ENABLED(CONFIG_ZMK_METEORITE_INPUT_PROCESSOR_MOTION_SCALER)
void zmk_meteorite_motion_scaler_reset_all(void);
#else
static inline void zmk_meteorite_motion_scaler_reset_all(void) {}
#endif

#ifdef __cplusplus
}
#endif
