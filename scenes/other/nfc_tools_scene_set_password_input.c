#include "../../include/nfc_tools_i.h"

// Input for the password to apply to the NTAG21x.
// The text is stored in ndef_buf1 and will be converted to 4 bytes
// via the first bytes of an MD5 hash at write time.

static void nfc_tools_scene_set_password_input_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_set_password_input_on_enter(void* context) {
    NfcToolsApp* app = context;
    TextInput*   ti  = app->text_input;

    text_input_set_header_text(ti, NTS_INPUT_PASSWORD);
    app->ndef_buf1[0] = '\0';

    text_input_set_result_callback(
        ti,
        nfc_tools_scene_set_password_input_callback,
        app,
        app->ndef_buf1,
        sizeof(app->ndef_buf1),
        false);

    text_input_set_minimum_length(ti, 1);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextInput);
}

bool nfc_tools_scene_set_password_input_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdSetPasswordWrite);
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_set_password_input_on_exit(void* context) {
    NfcToolsApp* app = context;
    text_input_reset(app->text_input);
}
