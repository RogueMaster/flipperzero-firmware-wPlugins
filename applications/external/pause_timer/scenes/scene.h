#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) PauseTimerScene##id,
typedef enum {
#include "scene_config.h"
    PauseTimerSceneNum,
} PauseTimerScene;
#undef ADD_SCENE

extern const SceneManagerHandlers pause_timer_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "scene_config.h"
#undef ADD_SCENE
