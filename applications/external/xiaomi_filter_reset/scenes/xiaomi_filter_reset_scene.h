// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file xiaomi_filter_reset_scene.h
 * @brief Scene identifiers and handler declarations (generated via X-macros).
 */
#pragma once

#include <gui/scene_manager.h>

/** @brief Scene identifiers. */
typedef enum {
#define ADD_SCENE(prefix, name, id) XiaomiFilterResetScene##id,
#include "xiaomi_filter_reset_scene_config.h"
#undef ADD_SCENE
    XiaomiFilterResetSceneNum,
} XiaomiFilterResetScene;

extern const SceneManagerHandlers xiaomi_filter_reset_scene_handlers;

#define ADD_SCENE(prefix, name, id)                                            \
    void prefix##_scene_##name##_on_enter(void* context);                      \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent e); \
    void prefix##_scene_##name##_on_exit(void* context);
#include "xiaomi_filter_reset_scene_config.h"
#undef ADD_SCENE
