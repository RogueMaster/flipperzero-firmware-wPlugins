#pragma once

#include <gui/scene_manager.h>

// Generate the scene id enum
#define ADD_SCENE(prefix, name, id) FaradayScene##id,
typedef enum {
#include "faraday_scene_config.h"
    FaradaySceneNum,
} FaradayScene;
#undef ADD_SCENE

extern const SceneManagerHandlers faraday_scene_handlers;

// Generate scene handler prototypes
#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void* context);                          \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "faraday_scene_config.h"
#undef ADD_SCENE
