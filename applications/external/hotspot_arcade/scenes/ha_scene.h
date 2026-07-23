#pragma once

#include <gui/scene_manager.h>

// Scene id enum
#define ADD_SCENE(prefix, name, id) HaScene##id,
typedef enum {
#include "ha_scene_config.h"
    HaSceneNum,
} HaScene;
#undef ADD_SCENE

extern const SceneManagerHandlers ha_scene_handlers;

// Handler prototypes
#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void*);                                  \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "ha_scene_config.h"
#undef ADD_SCENE
