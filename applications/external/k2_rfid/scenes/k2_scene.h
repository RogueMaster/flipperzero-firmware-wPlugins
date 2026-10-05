#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) K2Scene##id,
typedef enum {
#include "k2_scene_config.h"
    K2SceneCount,
} K2Scene;
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "k2_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "k2_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "k2_scene_config.h"
#undef ADD_SCENE

extern const SceneManagerHandlers k2_scene_handlers;
