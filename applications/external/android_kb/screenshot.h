#pragma once

#include "android_keyboard_bridge.h"

/**
 * Optional on-device screenshot (triple short Down).
 *
 * Enabled at build time when AKB_SCREENSHOT is defined, e.g.:
 *   AKB_SCREENSHOT=1 make flipper-build
 *   AKB_SCREENSHOT=1 make flipper-launch
 *
 * Saves PBM (P4) to /ext/apps_data/android_keyboard_bridge/shot_*.pbm
 * Convert later:  magick shot_….pbm shot.png
 */

#ifdef AKB_SCREENSHOT

void akb_screenshot_init(AndroidKeyboardBridge* app);
void akb_screenshot_free(AndroidKeyboardBridge* app);
/** Call from input pubsub on InputKeyDown + InputTypeShort. */
void akb_screenshot_on_down_short(AndroidKeyboardBridge* app);
/** Call from the main loop to flush a pending capture to the SD card. */
void akb_screenshot_poll(AndroidKeyboardBridge* app);

#else

static inline void akb_screenshot_init(AndroidKeyboardBridge* app) {
    UNUSED(app);
}
static inline void akb_screenshot_free(AndroidKeyboardBridge* app) {
    UNUSED(app);
}
static inline void akb_screenshot_on_down_short(AndroidKeyboardBridge* app) {
    UNUSED(app);
}
static inline void akb_screenshot_poll(AndroidKeyboardBridge* app) {
    UNUSED(app);
}

#endif
