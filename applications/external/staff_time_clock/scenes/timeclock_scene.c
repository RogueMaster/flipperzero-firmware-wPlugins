// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Vladyslav Pereverzyev

#include "timeclock_scene.h"

// Generate the three handler tables from the X-macro list.

void (*const timeclock_scene_on_enter_handlers[])(void*) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
#include "timeclock_scene_config.h"
#undef ADD_SCENE
};

bool (*const timeclock_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
#include "timeclock_scene_config.h"
#undef ADD_SCENE
};

void (*const timeclock_scene_on_exit_handlers[])(void*) = {
#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
#include "timeclock_scene_config.h"
#undef ADD_SCENE
};

const SceneManagerHandlers timeclock_scene_handlers = {
    .on_enter_handlers = timeclock_scene_on_enter_handlers,
    .on_event_handlers = timeclock_scene_on_event_handlers,
    .on_exit_handlers = timeclock_scene_on_exit_handlers,
    .scene_num = TimeClockSceneNum,
};
