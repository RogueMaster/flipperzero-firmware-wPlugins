#include "../nfc_magic_app_i.h"

// The password is captured via the shared byte_input_store; it must fit.
_Static_assert(
    USCUID_UL_PWD_SIZE <= NFC_MAGIC_APP_BYTE_INPUT_STORE_SIZE,
    "PWD must fit byte_input_store");

void nfc_magic_scene_uscuid_ul_pwd_input_byte_input_callback(void* context) {
    NfcMagicApp* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, NfcMagicAppCustomEventByteInputDone);
}

void nfc_magic_scene_uscuid_ul_pwd_input_on_enter(void* context) {
    NfcMagicApp* instance = context;

    // Seed the editor with the current password (if any), then capture into the shared store;
    // it's only committed to the poller after the user confirms the warning.
    memcpy(instance->byte_input_store, instance->uscuid_ul_password, USCUID_UL_PWD_SIZE);

    ByteInput* byte_input = instance->byte_input;
    byte_input_set_header_text(byte_input, "Enter the password in hex");
    byte_input_set_result_callback(
        byte_input,
        nfc_magic_scene_uscuid_ul_pwd_input_byte_input_callback,
        NULL,
        instance,
        instance->byte_input_store,
        USCUID_UL_PWD_SIZE);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewByteInput);
}

bool nfc_magic_scene_uscuid_ul_pwd_input_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicAppCustomEventByteInputDone) {
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneUscuidUlPwdWarn);
            consumed = true;
        }
    }
    return consumed;
}

void nfc_magic_scene_uscuid_ul_pwd_input_on_exit(void* context) {
    NfcMagicApp* instance = context;
    byte_input_set_result_callback(instance->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(instance->byte_input, "");
}
