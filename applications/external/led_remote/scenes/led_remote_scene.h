#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) LedRemoteScene##name,
typedef enum {
#include "led_remote_scene_config.h"
    LedRemoteSceneNum,
} LedRemoteScene;
#undef ADD_SCENE

extern const SceneManagerHandlers led_remote_scene_handlers;

bool led_remote_scene_navigation_event_callback(void* context);
