#include "../../include/nfc_tools_i.h"

static void nfc_tools_scene_write_wifi_pass_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_wifi_pass_on_enter(void* context) {
    NfcToolsApp* app = context;
    TextInput* ti = app->text_input;

    text_input_set_header_text(ti, NTS_INPUT_WIFI_PASSWORD);

    text_input_set_result_callback(
        ti,
        nfc_tools_scene_write_wifi_pass_callback,
        app,
        app->ndef_buf2,
        sizeof(app->ndef_buf2),
        false);

    // Password may be empty (open network)
    text_input_set_minimum_length(ti, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextInput);
}

bool nfc_tools_scene_write_wifi_pass_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteScan);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_wifi_pass_on_exit(void* context) {
    NfcToolsApp* app = context;
    text_input_reset(app->text_input);
}
