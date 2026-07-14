#include "timed_remote_scene.h"

static void (*const scene_on_enter[])(void*) = {
    [SCENE_BROWSE] = scene_browse_enter,
    [SCENE_SELECT] = scene_select_enter,
    [SCENE_CONFIG] = scene_cfg_enter,
    [SCENE_RUN] = scene_run_enter,
    [SCENE_DONE] = scene_done_enter,
};

static bool (*const scene_on_event[])(void*, SceneManagerEvent) = {
    [SCENE_BROWSE] = scene_browse_event,
    [SCENE_SELECT] = scene_select_event,
    [SCENE_CONFIG] = scene_cfg_event,
    [SCENE_RUN] = scene_run_event,
    [SCENE_DONE] = scene_done_event,
};

static void (*const scene_on_exit[])(void*) = {
    [SCENE_BROWSE] = scene_browse_exit,
    [SCENE_SELECT] = scene_select_exit,
    [SCENE_CONFIG] = scene_cfg_exit,
    [SCENE_RUN] = scene_run_exit,
    [SCENE_DONE] = scene_done_exit,
};

const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers = scene_on_enter,
    .on_event_handlers = scene_on_event,
    .on_exit_handlers = scene_on_exit,
    .scene_num = SCENE_COUNT,
};
