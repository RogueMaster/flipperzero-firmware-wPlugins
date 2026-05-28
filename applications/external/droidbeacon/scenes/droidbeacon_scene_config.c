#include "droidbeacon_scene.h"

static void (*const droidbeacon_on_enter_handlers[])(void*) = {
    droidbeacon_scene_splash_on_enter,
    droidbeacon_scene_menu_on_enter,
    droidbeacon_scene_beaming_on_enter,
    droidbeacon_scene_result_on_enter,
};

static bool (*const droidbeacon_on_event_handlers[])(void*, SceneManagerEvent) = {
    droidbeacon_scene_splash_on_event,
    droidbeacon_scene_menu_on_event,
    droidbeacon_scene_beaming_on_event,
    droidbeacon_scene_result_on_event,
};

static void (*const droidbeacon_on_exit_handlers[])(void*) = {
    droidbeacon_scene_splash_on_exit,
    droidbeacon_scene_menu_on_exit,
    droidbeacon_scene_beaming_on_exit,
    droidbeacon_scene_result_on_exit,
};

const SceneManagerHandlers droidbeacon_scene_handlers = {
    .on_enter_handlers = droidbeacon_on_enter_handlers,
    .on_event_handlers = droidbeacon_on_event_handlers,
    .on_exit_handlers  = droidbeacon_on_exit_handlers,
    .scene_num         = DroidbeaconSceneCount,
};
