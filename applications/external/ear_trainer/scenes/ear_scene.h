#pragma once

#include <gui/scene_manager.h>

/* One list generates the scene id enum and the three handler tables, so they
 * can never drift out of sync. */
#define ADD_SCENE(name) EarScene_##name,
typedef enum {
#include "ear_scene_config.h"
    EarSceneNum,
} EarSceneIndex;
#undef ADD_SCENE

extern const SceneManagerHandlers ear_scene_handlers;

#define ADD_SCENE(name) void ear_scene_##name##_on_enter(void* context);
#include "ear_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(name) bool ear_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "ear_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(name) void ear_scene_##name##_on_exit(void* context);
#include "ear_scene_config.h"
#undef ADD_SCENE
