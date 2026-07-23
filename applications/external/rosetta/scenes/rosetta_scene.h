#pragma once

#include <gui/scene_manager.h>

// Generate the scene id enum
#define ADD_SCENE(prefix, name, id) RosettaScene##id,
typedef enum {
#include "rosetta_scene_config.h"
    RosettaSceneNum,
} RosettaScene;
#undef ADD_SCENE

extern const SceneManagerHandlers rosetta_scene_handlers;

// Generate scene handler prototypes
#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void* context);                          \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "rosetta_scene_config.h"
#undef ADD_SCENE
