#pragma once

#include <gui/scene_manager.h>

// Generate the scene id enum.
#define ADD_SCENE(prefix, name, id) CerberusScene##id,
typedef enum {
#include "cerberus_scene_config.h"
    CerberusSceneNum,
} CerberusScene;
#undef ADD_SCENE

extern const SceneManagerHandlers cerberus_scene_handlers;

// Generate the handler prototypes.
#define ADD_SCENE(prefix, name, id)                                      \
    void prefix##_scene_##name##_on_enter(void* context);                \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "cerberus_scene_config.h"
#undef ADD_SCENE
