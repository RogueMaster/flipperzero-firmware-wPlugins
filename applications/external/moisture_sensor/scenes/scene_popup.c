#include "../moisture_sensor.h"

static void popup_callback(void* context) {
    MoistureSensorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MoistureSensorEventPopupDone);
}

void moisture_sensor_scene_popup_on_enter(void* context) {
    MoistureSensorApp* app = context;

    popup_set_header(app->popup, app->popup_message, 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 1000);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, popup_callback);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, MoistureSensorViewPopup);
}

bool moisture_sensor_scene_popup_on_event(void* context, SceneManagerEvent event) {
    MoistureSensorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MoistureSensorEventPopupDone) {
            if(app->popup_return_to_main) {
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, MoistureSensorSceneMain);
            } else {
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
        }
    }
    return consumed;
}

void moisture_sensor_scene_popup_on_exit(void* context) {
    MoistureSensorApp* app = context;
    popup_reset(app->popup);
}
