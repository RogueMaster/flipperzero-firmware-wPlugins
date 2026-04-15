#pragma once

#include <gui/scene_manager.h>

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "cogs_mikai_scene_config.c"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "cogs_mikai_scene_config.c"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "cogs_mikai_scene_config.c"
#undef ADD_SCENE

// Generate scene configuration array
#define ADD_SCENE(prefix, name, id)                                                      \
    {.on_enter = prefix##_scene_##name##_on_enter, .on_event = prefix##_scene_##name##_on_event, \
     .on_exit = prefix##_scene_##name##_on_exit},

extern const SceneManagerHandlers cogs_mikai_scene_handlers;
