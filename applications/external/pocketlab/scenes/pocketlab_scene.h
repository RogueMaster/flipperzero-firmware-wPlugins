#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) PocketLabScene##id,
typedef enum {
#include "pocketlab_scene_config.h"
    PocketLabSceneNum,
} PocketLabScene;
#undef ADD_SCENE

extern const SceneManagerHandlers pocketlab_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "pocketlab_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "pocketlab_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "pocketlab_scene_config.h"
#undef ADD_SCENE
