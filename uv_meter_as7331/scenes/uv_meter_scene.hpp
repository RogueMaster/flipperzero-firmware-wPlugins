#pragma once

#include <gui/scene_manager.h>

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) UVMeterScene##id,
typedef enum {
#include "uv_meter_scene_config.hpp"
    UVMeterSceneNum,
} UVMeterScene;
#undef ADD_SCENE

extern const SceneManagerHandlers uv_meter_scene_handlers;

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "uv_meter_scene_config.hpp"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "uv_meter_scene_config.hpp"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "uv_meter_scene_config.hpp"
#undef ADD_SCENE