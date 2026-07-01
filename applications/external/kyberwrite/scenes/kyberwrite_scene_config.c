#include "kyberwrite_scene.h"

static void (*const kyberwrite_on_enter_handlers[])(void*) = {
    kyberwrite_scene_splash_on_enter,
    kyberwrite_scene_hint_on_enter,
    kyberwrite_scene_menu_on_enter,
    kyberwrite_scene_color_on_enter,
    kyberwrite_scene_writing_on_enter,
    kyberwrite_scene_result_on_enter,
    kyberwrite_scene_read_on_enter,
};

static bool (*const kyberwrite_on_event_handlers[])(void*, SceneManagerEvent) = {
    kyberwrite_scene_splash_on_event,
    kyberwrite_scene_hint_on_event,
    kyberwrite_scene_menu_on_event,
    kyberwrite_scene_color_on_event,
    kyberwrite_scene_writing_on_event,
    kyberwrite_scene_result_on_event,
    kyberwrite_scene_read_on_event,
};

static void (*const kyberwrite_on_exit_handlers[])(void*) = {
    kyberwrite_scene_splash_on_exit,
    kyberwrite_scene_hint_on_exit,
    kyberwrite_scene_menu_on_exit,
    kyberwrite_scene_color_on_exit,
    kyberwrite_scene_writing_on_exit,
    kyberwrite_scene_result_on_exit,
    kyberwrite_scene_read_on_exit,
};

const SceneManagerHandlers kyberwrite_scene_handlers = {
    .on_enter_handlers = kyberwrite_on_enter_handlers,
    .on_event_handlers = kyberwrite_on_event_handlers,
    .on_exit_handlers  = kyberwrite_on_exit_handlers,
    .scene_num         = KyberSceneCount,
};
