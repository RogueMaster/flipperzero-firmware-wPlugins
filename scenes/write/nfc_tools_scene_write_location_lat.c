#include "../../include/nfc_tools_i.h"

static void nfc_tools_scene_write_location_lat_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_location_lat_on_enter(void* context) {
    NfcToolsApp* app   = context;
    EmailInput* input = app->email_input;
    email_input_set_header_text(input, NTS_INPUT_LATITUDE);
    email_input_set_result_callback(
        input, nfc_tools_scene_write_location_lat_callback, app,
        app->ndef_buf1, sizeof(app->ndef_buf1), false);
    email_input_set_minimum_length(input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewEmailInput);
}

bool nfc_tools_scene_write_location_lat_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteLocationLon);
        consumed = true;
    }
    return consumed;
}

void nfc_tools_scene_write_location_lat_on_exit(void* context) {
    NfcToolsApp* app = context;
    email_input_reset(app->email_input);
}
