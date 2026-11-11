#pragma once

#include <gui/scene_manager.h>

/* Scene IDs */
#define ADD_SCENE(prefix, name, id) HamCramScene##id,
typedef enum {
#include "hamcram_scene_config.h"
    HamCramSceneNum,
} HamCramScene;
#undef ADD_SCENE

extern const SceneManagerHandlers hamcram_scene_handlers;

/* Handler declarations */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "hamcram_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "hamcram_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "hamcram_scene_config.h"
#undef ADD_SCENE
