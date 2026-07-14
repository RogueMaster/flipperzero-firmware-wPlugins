// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene.c
 * @brief Scene handler tables (generated via X-macros).
 */
#include "xiaomi_filter_reset_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const xiaomi_filter_reset_scene_on_enter_handlers[])(void*) = {
#include "xiaomi_filter_reset_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const xiaomi_filter_reset_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
#include "xiaomi_filter_reset_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const xiaomi_filter_reset_scene_on_exit_handlers[])(void*) = {
#include "xiaomi_filter_reset_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers xiaomi_filter_reset_scene_handlers = {
    .on_enter_handlers = xiaomi_filter_reset_scene_on_enter_handlers,
    .on_event_handlers = xiaomi_filter_reset_scene_on_event_handlers,
    .on_exit_handlers = xiaomi_filter_reset_scene_on_exit_handlers,
    .scene_num = XiaomiFilterResetSceneNum,
};
