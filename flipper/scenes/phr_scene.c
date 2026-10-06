#include "phr_scene.h"

static void (*const on_enter_handlers[])(void*) = {
    phr_scene_main_on_enter,
    phr_scene_alert_on_enter,
    phr_scene_settings_on_enter,
    phr_scene_rule_on_enter,
};

static bool (*const on_event_handlers[])(void*, SceneManagerEvent) = {
    phr_scene_main_on_event,
    phr_scene_alert_on_event,
    phr_scene_settings_on_event,
    phr_scene_rule_on_event,
};

static void (*const on_exit_handlers[])(void*) = {
    phr_scene_main_on_exit,
    phr_scene_alert_on_exit,
    phr_scene_settings_on_exit,
    phr_scene_rule_on_exit,
};

const SceneManagerHandlers phr_scene_handlers = {
    .on_enter_handlers = on_enter_handlers,
    .on_event_handlers = on_event_handlers,
    .on_exit_handlers = on_exit_handlers,
    .scene_num = PhrSceneCount,
};
