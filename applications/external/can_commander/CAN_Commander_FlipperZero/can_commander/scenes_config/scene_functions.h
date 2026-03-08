#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) prefix##_scene_##id,
typedef enum {
#include "scene_config.h"
    cancommander_scene_num,
} CanCommanderScene;
#undef ADD_SCENE

extern const SceneManagerHandlers cancommander_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##id##_on_enter(void* context);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##id##_on_event(void* context, SceneManagerEvent event);
#include "scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##id##_on_exit(void* context);
#include "scene_config.h"
#undef ADD_SCENE
