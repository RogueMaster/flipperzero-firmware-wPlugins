#include "../../include/nfc_tools_i.h"

static void nfc_tools_scene_write_custom_mime_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_custom_mime_on_enter(void* context) {
    NfcToolsApp* app   = context;
    MimeInput* input = app->mime_input;
    mime_input_set_header_text(input, NTS_INPUT_CONTENT_TYPE);
    mime_input_set_result_callback(
        input, nfc_tools_scene_write_custom_mime_callback, app,
        app->ndef_buf1, sizeof(app->ndef_buf1), false);
    mime_input_set_minimum_length(input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewMimeInput);
}

bool nfc_tools_scene_write_custom_mime_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteCustomData);
        consumed = true;
    }
    return consumed;
}

void nfc_tools_scene_write_custom_mime_on_exit(void* context) {
    NfcToolsApp* app = context;
    mime_input_reset(app->mime_input);
}
