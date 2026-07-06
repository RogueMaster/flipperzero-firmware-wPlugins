#pragma once

#include <gui/scene_manager.h>

// Scene id enum
#define ADD_SCENE(prefix, name, id) CfsScene##id,
typedef enum {
#include "src/scenes/scene_config.h"
    CfsSceneNum,
} CfsScene;
#undef ADD_SCENE

extern const SceneManagerHandlers cfs_scene_handlers;

// Per-scene handler prototypes
#define ADD_SCENE(prefix, name, id)                                            \
    void prefix##_scene_##name##_on_enter(void* context);                      \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent e); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "src/scenes/scene_config.h"
#undef ADD_SCENE
