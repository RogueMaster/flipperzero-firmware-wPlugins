/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The three handler tables the scene manager dispatches through, generated
 * from the one list in emrtd_scene_config.h so that a scene cannot be added
 * to the enum and forgotten in the table.
 */
#include "emrtd_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const emrtd_on_enter_handlers[])(void*) = {
#include "emrtd_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const emrtd_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "emrtd_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const emrtd_on_exit_handlers[])(void* context) = {
#include "emrtd_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers emrtd_scene_handlers = {
    .on_enter_handlers = emrtd_on_enter_handlers,
    .on_event_handlers = emrtd_on_event_handlers,
    .on_exit_handlers = emrtd_on_exit_handlers,
    .scene_num = EmrtdSceneCount,
};
