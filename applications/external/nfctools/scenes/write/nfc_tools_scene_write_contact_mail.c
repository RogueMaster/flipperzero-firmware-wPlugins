#include "../../include/nfc_tools_i.h"

// Clavier email : @  .  -  disponibles

static void nfc_tools_scene_write_contact_mail_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_contact_mail_on_enter(void* context) {
    NfcToolsApp* app = context;
    Keyboard* ei = app->keyboard;

    keyboard_set_header_text(ei, NTS_INPUT_EMAIL);

    keyboard_set_result_callback(
        ei,
        nfc_tools_scene_write_contact_mail_callback,
        app,
        app->ndef_buf5,
        sizeof(app->ndef_buf5),
        false);

    keyboard_set_minimum_length(ei, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewKeyboard);
}

bool nfc_tools_scene_write_contact_mail_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteContactUrl);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_contact_mail_on_exit(void* context) {
    NfcToolsApp* app = context;
    keyboard_reset(app->keyboard);
}
