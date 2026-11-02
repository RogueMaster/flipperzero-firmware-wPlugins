/**
 * @file fn_scene.h
 * @brief Реестр сцен приложения (единый список — fn_scene_config.h).
 */
#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) FNAppScene##id,
typedef enum {
#include "fn_scene_config.h"
    FNAppSceneNum,
} FNAppScene;
#undef ADD_SCENE

extern const SceneManagerHandlers fn_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "fn_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "fn_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "fn_scene_config.h"
#undef ADD_SCENE
