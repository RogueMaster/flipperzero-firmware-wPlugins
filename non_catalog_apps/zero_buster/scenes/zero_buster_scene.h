#pragma once

#include <furi.h>
#include <input/input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZeroBusterApp ZeroBusterApp;

bool zero_buster_scene_main_on_event(ZeroBusterApp* app, InputEvent* event);

bool zero_buster_scene_edit_hid_on_event(ZeroBusterApp* app, InputEvent* event);

bool zero_buster_scene_status_on_event(ZeroBusterApp* app, InputEvent* event);

bool zero_buster_scene_about_on_event(ZeroBusterApp* app, InputEvent* event);

#ifdef __cplusplus
}
#endif
