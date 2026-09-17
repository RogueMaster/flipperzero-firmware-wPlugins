// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#pragma once

#include <gui/scene_manager.h>

// Scene enum generated from the X-macro list. The enum prefix (TimeClockScene)
// is fixed here; the passed `prefix` (timeclock) is used only for the handler
// function names below.
typedef enum {
#define ADD_SCENE(prefix, name, id) TimeClockScene##id,
#include "timeclock_scene_config.h"
#undef ADD_SCENE
    TimeClockSceneNum,
} TimeClockScene;

extern const SceneManagerHandlers timeclock_scene_handlers;

// on_enter handlers
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void* context);
#include "timeclock_scene_config.h"
#undef ADD_SCENE

// on_event handlers
#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "timeclock_scene_config.h"
#undef ADD_SCENE

// on_exit handlers
#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "timeclock_scene_config.h"
#undef ADD_SCENE
