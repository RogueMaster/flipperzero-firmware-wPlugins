#include "../moisture_sensor.h"

// Scene handler arrays for SceneManager
static void (*const scene_on_enter_handlers[])(void*) = {
    moisture_sensor_scene_main_on_enter,
    moisture_sensor_scene_menu_on_enter,
    moisture_sensor_scene_popup_on_enter,
};

static bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    moisture_sensor_scene_main_on_event,
    moisture_sensor_scene_menu_on_event,
    moisture_sensor_scene_popup_on_event,
};

static void (*const scene_on_exit_handlers[])(void*) = {
    moisture_sensor_scene_main_on_exit,
    moisture_sensor_scene_menu_on_exit,
    moisture_sensor_scene_popup_on_exit,
};

const SceneManagerHandlers moisture_sensor_scene_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = MoistureSensorSceneCount,
};
