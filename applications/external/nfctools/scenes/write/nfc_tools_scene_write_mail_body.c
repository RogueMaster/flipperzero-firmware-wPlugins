#include "../../include/nfc_tools_i.h"

// Input for the (optional) message body of the NDEF mail.
// Stored in ndef_buf3. Minimum length = 0 (optional field).

static void nfc_tools_scene_write_mail_body_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_mail_body_on_enter(void* context) {
    NfcToolsApp* app = context;
    Keyboard* ti = app->keyboard;

    keyboard_set_header_text(ti, NTS_INPUT_MESSAGE_OPT);

    keyboard_set_result_callback(
        ti,
        nfc_tools_scene_write_mail_body_callback,
        app,
        app->ndef_buf3,
        sizeof(app->ndef_buf3),
        false);

    keyboard_set_minimum_length(ti, 0);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewKeyboard);
}

bool nfc_tools_scene_write_mail_body_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        // All fields entered → write the tag
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteScan);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_mail_body_on_exit(void* context) {
    NfcToolsApp* app = context;
    keyboard_reset(app->keyboard);
}
