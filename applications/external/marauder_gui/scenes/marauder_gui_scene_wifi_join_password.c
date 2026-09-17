#include "../marauder_gui_app_i.h"

enum {
    WifiJoinPasswordEventDone,
};

static void marauder_gui_scene_wifi_join_password_callback(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WifiJoinPasswordEventDone);
}

void marauder_gui_scene_wifi_join_password_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_text_input_set_header_text(
        app->text_input, marauder_gui_text(app, "WiFi Sifresi", "WiFi Password"));
    marauder_text_input_set_result_callback(
        app->text_input,
        marauder_gui_scene_wifi_join_password_callback,
        app,
        app->wifi_join_password,
        sizeof(app->wifi_join_password),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewTextInput);
}

bool marauder_gui_scene_wifi_join_password_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == WifiJoinPasswordEventDone) {
        app->wifi_join_use_saved = false;
        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiJoinStatus);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_join_password_on_exit(void* context) {
    MarauderGuiApp* app = context;
    marauder_text_input_reset(app->text_input);
}
