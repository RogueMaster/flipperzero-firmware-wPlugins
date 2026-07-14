#ifndef LORA_SCENE_H
#define LORA_SCENE_H
#include <gui/scene_manager.h>

// Generate scene id and total number
#define ADD_SCENE(prefix, name, id) LoraScene##id,
typedef enum {
#include "lora_scene_config.h"
    LoraSceneNum,
} LoraScene;
#undef ADD_SCENE

// Generate scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "lora_scene_config.h"
#undef ADD_SCENE

// Generate scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "lora_scene_config.h"
#undef ADD_SCENE

// Generate scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "lora_scene_config.h"
#undef ADD_SCENE

extern const SceneManagerHandlers lora_scene_handlers;

#endif
