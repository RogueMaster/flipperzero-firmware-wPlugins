#pragma once

#include <gui/scene_manager.h>

// Generate scene id and handlers declaration
#define ADD_SCENE(prefix, name, id) BtAudioScene##id,
typedef enum {
#include "bt_audio_scene_config.h"
    BtAudioSceneNum,
} BtAudioScene;
#undef ADD_SCENE

extern const SceneManagerHandlers bt_audio_scene_handlers;

// Scene on_enter handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "bt_audio_scene_config.h"
#undef ADD_SCENE

// Scene on_event handlers declaration
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "bt_audio_scene_config.h"
#undef ADD_SCENE

// Scene on_exit handlers declaration
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "bt_audio_scene_config.h"
#undef ADD_SCENE
