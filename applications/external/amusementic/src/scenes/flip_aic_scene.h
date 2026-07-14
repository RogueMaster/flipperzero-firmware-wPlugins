#pragma once

#include <gui/scene_manager.h>

// Generate scene id and total number
#define ADD_SCENE(name, id) FlipAICScene##id,
typedef enum {
#include "flip_aic_scene_config.h"
    FlipAICSceneNum,
} FlipAICScene;
#undef ADD_SCENE

extern const SceneManagerHandlers flip_aic_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(name, id) void flip_aic_scene_##name##_on_enter(void*);
#include "flip_aic_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(name, id) \
    bool flip_aic_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "flip_aic_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(name, id) void flip_aic_scene_##name##_on_exit(void* context);
#include "flip_aic_scene_config.h"
#undef ADD_SCENE
