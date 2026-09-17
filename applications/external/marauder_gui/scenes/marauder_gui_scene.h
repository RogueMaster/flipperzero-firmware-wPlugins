#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) MarauderGuiScene##id,
typedef enum {
#include "marauder_gui_scene_config.h"
    MarauderGuiSceneNum,
} MarauderGuiScene;
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "marauder_gui_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "marauder_gui_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "marauder_gui_scene_config.h"
#undef ADD_SCENE

extern const SceneManagerHandlers marauder_gui_scene_handlers;
