#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) GameScene##id,
typedef enum {
#include "game_scene_config.h"
    GameSceneNum,
} GameScene;
#undef ADD_SCENE

extern const SceneManagerHandlers game_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "game_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "game_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "game_scene_config.h"
#undef ADD_SCENE
