#include "../../include/nfc_tools_i.h"

// Clavier email : @  .  -  disponibles

static void nfc_tools_scene_write_contact_mail_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_contact_mail_on_enter(void* context) {
    NfcToolsApp* app = context;
    EmailInput*  ei  = app->email_input;

    email_input_set_header_text(ei, NTS_INPUT_EMAIL);

    email_input_set_result_callback(
        ei,
        nfc_tools_scene_write_contact_mail_callback,
        app,
        app->ndef_buf5,
        sizeof(app->ndef_buf5),
        false);

    email_input_set_minimum_length(ei, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewEmailInput);
}

bool nfc_tools_scene_write_contact_mail_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteContactUrl);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_contact_mail_on_exit(void* context) {
    NfcToolsApp* app = context;
    email_input_reset(app->email_input);
}
