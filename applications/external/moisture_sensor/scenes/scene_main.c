#include "../moisture_sensor.h"

static void sensor_view_menu_callback(void* context) {
    MoistureSensorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MoistureSensorEventMenuSelected);
}

void moisture_sensor_scene_main_on_enter(void* context) {
    MoistureSensorApp* app = context;
    sensor_view_set_menu_callback(app->sensor_view, sensor_view_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, MoistureSensorViewSensor);
}

bool moisture_sensor_scene_main_on_event(void* context, SceneManagerEvent event) {
    MoistureSensorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MoistureSensorEventMenuSelected) {
            app->edit_dry_value = app->cal_dry_value;
            app->edit_wet_value = app->cal_wet_value;
            scene_manager_next_scene(app->scene_manager, MoistureSensorSceneMenu);
            consumed = true;
        }
    }
    return consumed;
}

void moisture_sensor_scene_main_on_exit(void* context) {
    UNUSED(context);
}
