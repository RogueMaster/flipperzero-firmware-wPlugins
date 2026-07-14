#pragma once

#include <gui/scene_manager.h>
#include "../droidbeacon_app.h"

// Event enum lives in droidbeacon_app.h — do not redeclare here

void droidbeacon_scene_splash_on_enter(void* context);
bool droidbeacon_scene_splash_on_event(void* context, SceneManagerEvent event);
void droidbeacon_scene_splash_on_exit(void* context);

void droidbeacon_scene_menu_on_enter(void* context);
bool droidbeacon_scene_menu_on_event(void* context, SceneManagerEvent event);
void droidbeacon_scene_menu_on_exit(void* context);

void droidbeacon_scene_beaming_on_enter(void* context);
bool droidbeacon_scene_beaming_on_event(void* context, SceneManagerEvent event);
void droidbeacon_scene_beaming_on_exit(void* context);

void droidbeacon_scene_result_on_enter(void* context);
bool droidbeacon_scene_result_on_event(void* context, SceneManagerEvent event);
void droidbeacon_scene_result_on_exit(void* context);

extern const SceneManagerHandlers droidbeacon_scene_handlers;
