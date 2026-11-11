#pragma once

#include <gui/scene_manager.h>

// Generate scene id enum and total number
#define ADD_SCENE(prefix, name, id) IrMakerScene##id,
typedef enum {
#include "ir_maker_scene_config.h"
    IrMakerSceneNum,
} IrMakerScene;
#undef ADD_SCENE

extern const SceneManagerHandlers ir_maker_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "ir_maker_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "ir_maker_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "ir_maker_scene_config.h"
#undef ADD_SCENE
