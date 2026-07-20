#pragma once

#include <zephyr/sys/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Drop scroll transform history on the next input event. Ball-profile routing
 * calls this on every profile transition so sub-step motion and Adaptive gain
 * never leak across SCROLL entry/exit. */
#if IS_ENABLED(CONFIG_ZMK_METEORITE_INPUT_PROCESSOR_SCROLL_TRANSFORM)
void zmk_meteorite_scroll_transform_reset_all(void);
#else
static inline void zmk_meteorite_scroll_transform_reset_all(void) {}
#endif

#ifdef __cplusplus
}
#endif
