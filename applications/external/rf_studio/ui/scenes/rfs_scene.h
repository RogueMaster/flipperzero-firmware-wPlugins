#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) RfsScene##id,
typedef enum {
#include "rfs_scene_config.h"
    RfsSceneNum,
} RfsScene;
#undef ADD_SCENE

extern const SceneManagerHandlers rfs_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "rfs_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "rfs_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "rfs_scene_config.h"
#undef ADD_SCENE
