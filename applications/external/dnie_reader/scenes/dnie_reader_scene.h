#pragma once

#include <gui/scene_manager.h>

// Genera el enum DnieScene{Start,CanInput,Read,Result,...}_Num
#define ADD_SCENE(prefix, name, id) DnieScene##id,
typedef enum {
#include "dnie_reader_scene_config.h"
    DnieSceneNum,
} DnieScene;
#undef ADD_SCENE

extern const SceneManagerHandlers dnie_scene_handlers;

// Prototipos on_enter / on_event / on_exit de cada escena
#define ADD_SCENE(prefix, name, id)                                                \
    void prefix##_scene_##name##_on_enter(void*);                                  \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "dnie_reader_scene_config.h"
#undef ADD_SCENE
