/* SPDX-License-Identifier: GPL-3.0-only */
#include "ssh_runner_scene.h"

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_enter,
static void (*const ssh_runner_on_enter_handlers[])(void*) = {
#include "ssh_runner_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_event,
static bool (*const ssh_runner_on_event_handlers[])(void* context, SceneManagerEvent event) = {
#include "ssh_runner_scene_config.h"
};
#undef ADD_SCENE

#define ADD_SCENE(prefix, name, id) prefix##_scene_##name##_on_exit,
static void (*const ssh_runner_on_exit_handlers[])(void* context) = {
#include "ssh_runner_scene_config.h"
};
#undef ADD_SCENE

const SceneManagerHandlers ssh_runner_scene_handlers = {
    .on_enter_handlers = ssh_runner_on_enter_handlers,
    .on_event_handlers = ssh_runner_on_event_handlers,
    .on_exit_handlers = ssh_runner_on_exit_handlers,
    .scene_num = SshRunnerSceneNum,
};
