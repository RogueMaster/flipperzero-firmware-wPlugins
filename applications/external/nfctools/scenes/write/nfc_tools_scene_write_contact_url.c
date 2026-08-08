#include "../../include/nfc_tools_i.h"

// Clavier MIME : /  .  -  disponibles — indispensables pour une URL

static void nfc_tools_scene_write_contact_url_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_contact_url_on_enter(void* context) {
    NfcToolsApp* app = context;
    Keyboard* mi = app->keyboard;

    keyboard_set_header_text(mi, NTS_INPUT_WEBSITE);

    keyboard_set_result_callback(
        mi,
        nfc_tools_scene_write_contact_url_callback,
        app,
        app->ndef_buf6,
        sizeof(app->ndef_buf6),
        false);

    keyboard_set_minimum_length(mi, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewKeyboard);
}

bool nfc_tools_scene_write_contact_url_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteScan);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_write_contact_url_on_exit(void* context) {
    NfcToolsApp* app = context;
    keyboard_reset(app->keyboard);
}
