/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <gui/scene_manager.h>

#define ADD_SCENE(prefix, name, id) SshRunnerScene##id,
typedef enum {
#include "ssh_runner_scene_config.h"
    SshRunnerSceneNum,
} SshRunnerScene;
#undef ADD_SCENE

extern const SceneManagerHandlers ssh_runner_scene_handlers;

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_enter(void*);
#include "ssh_runner_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) \
    bool prefix##_scene_##name##_on_event(void* context, SceneManagerEvent event);
#include "ssh_runner_scene_config.h"
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) void prefix##_scene_##name##_on_exit(void* context);
#include "ssh_runner_scene_config.h"
#undef ADD_SCENE
