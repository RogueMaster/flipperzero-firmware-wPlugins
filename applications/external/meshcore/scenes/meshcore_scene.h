#pragma once

#include <gui/scene_manager.h>

/* Scene ids: MeshCoreSceneMenu, ... */
#define ADD_SCENE(prefix, name, id) MeshCoreScene##id,
typedef enum {
#include "meshcore_scene_config.h"
    MeshCoreSceneNum,
} MeshCoreSceneId;
#undef ADD_SCENE

extern const SceneManagerHandlers meshcore_scene_handlers;

/* Per-scene handler prototypes: meshcore_scene_menu_on_enter(), ... */
#define ADD_SCENE(prefix, name, id)                                    \
    void prefix##_scene_##name##_on_enter(void* context);              \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "meshcore_scene_config.h"
#undef ADD_SCENE
