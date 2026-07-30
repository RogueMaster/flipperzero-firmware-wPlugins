#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) WolScene##id,
typedef enum {
#include "wol_scene_config.h"
    WolSceneNum,
} WolScene;
#undef ADD_SCENE

extern const SceneManagerHandlers wol_scene_handlers;

void wol_scene_on_enter_handler(void* context, uint32_t index);
bool wol_scene_on_event_handler(void* context, SceneManagerEvent event);
void wol_scene_on_exit_handler(void* context, uint32_t index);

#define ADD_SCENE(prefix, name, id)                                            \
    void prefix##_scene_##name##_on_enter(void* context);                      \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent e); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "wol_scene_config.h"
#undef ADD_SCENE
