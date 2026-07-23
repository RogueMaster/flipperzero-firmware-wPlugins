#pragma once

#include <gui/scene_manager.h>

/* Generate the scene id enum */
#define ADD_SCENE(prefix, name, id) HermesScene##id,
typedef enum {
#include "hermes_scene_config.h"
    HermesSceneNum,
} HermesScene;
#undef ADD_SCENE

extern const SceneManagerHandlers hermes_scene_handlers;

/* Generate the scene handler prototypes */
#define ADD_SCENE(prefix, name, id)                                  \
    void prefix##_scene_##name##_on_enter(void*);                    \
    bool prefix##_scene_##name##_on_event(void*, SceneManagerEvent); \
    void prefix##_scene_##name##_on_exit(void*);
#include "hermes_scene_config.h"
#undef ADD_SCENE
