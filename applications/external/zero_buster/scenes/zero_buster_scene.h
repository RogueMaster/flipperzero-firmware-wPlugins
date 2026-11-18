#pragma once

#include <furi.h>
#include <input/input.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct ZeroBusterApp ZeroBusterApp;

/* Main menu scene: handles navigation, configuration cycling, and launching */
bool zero_buster_scene_main_on_event(ZeroBusterApp* app, InputEvent* event);

/* Custom HID editor scene: handles cursor movement and nibble/preset changes */
bool zero_buster_scene_edit_hid_on_event(ZeroBusterApp* app, InputEvent* event);

/* Status / Execution scene: handles cancel during payload injection & exfiltration */
bool zero_buster_scene_status_on_event(ZeroBusterApp* app, InputEvent* event);

/* About scene: displays credits and social links */
bool zero_buster_scene_about_on_event(ZeroBusterApp* app, InputEvent* event);

#ifdef __cplusplus
}
#endif
