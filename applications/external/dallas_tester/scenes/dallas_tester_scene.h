#pragma once

#include <gui/scene_manager.h>

// Generate scene id enum and total number
#define ADD_SCENE(prefix, name, id) DallasTesterScene##id,
typedef enum {
#include "dallas_tester_scene_config.h"
    DallasTesterSceneNum,
} DallasTesterScene;
#undef ADD_SCENE

extern const SceneManagerHandlers dallas_tester_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "dallas_tester_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "dallas_tester_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "dallas_tester_scene_config.h"
#undef ADD_SCENE
