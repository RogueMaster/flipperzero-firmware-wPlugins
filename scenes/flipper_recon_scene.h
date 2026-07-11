#pragma once

#include <gui/scene_manager.h>

/* Generate the scene id enumeration. */
#define ADD_SCENE(prefix, name, id) FlipperReconScene##id,
typedef enum {
#include "flipper_recon_scene_config.h"
    FlipperReconSceneCount,
} FlipperReconScene;
#undef ADD_SCENE

extern const SceneManagerHandlers flipper_recon_scene_handlers;

/* Generate scene handler prototypes. */
#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void* context);                          \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "flipper_recon_scene_config.h"
#undef ADD_SCENE
