/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 */
#pragma once

#include <gui/scene_manager.h>

/* Generate the scene id enum. */
#define ADD_SCENE(prefix, name, id) EmrtdScene##id,
typedef enum {
#include "emrtd_scene_config.h"
    EmrtdSceneCount,
} EmrtdScene;
#undef ADD_SCENE

extern const SceneManagerHandlers emrtd_scene_handlers;

/* Generate the handler prototypes. */
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "emrtd_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "emrtd_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "emrtd_scene_config.h"
#undef ADD_SCENE
