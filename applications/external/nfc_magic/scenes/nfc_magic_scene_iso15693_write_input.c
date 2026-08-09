#include "../nfc_magic_app_i.h"
#include "../magic/protocols/iso15693/iso15693_poller.h"

static void nfc_magic_scene_iso15693_write_input_callback(void* context) {
    NfcMagicApp* instance = context;
    view_dispatcher_send_custom_event(
        instance->view_dispatcher, NfcMagicAppCustomEventByteInputDone);
}

void nfc_magic_scene_iso15693_write_input_on_enter(void* context) {
    NfcMagicApp* instance = context;

    // Seed the editor with the card's current UID if it has already been read (via "Info"),
    // otherwise start from a valid E0-prefixed template.
    memcpy(
        instance->iso15693_target_uid,
        instance->iso15693_data->iso15693_3_data->uid,
        ISO15693_3_UID_SIZE);
    if(instance->iso15693_target_uid[0] != 0xE0) {
        memset(instance->iso15693_target_uid, 0, ISO15693_3_UID_SIZE);
        instance->iso15693_target_uid[0] = 0xE0;
    }

    ByteInput* byte_input = instance->byte_input;
    byte_input_set_header_text(byte_input, "Enter UID (starts with E0)");
    byte_input_set_result_callback(
        byte_input,
        nfc_magic_scene_iso15693_write_input_callback,
        NULL,
        instance,
        instance->iso15693_target_uid,
        ISO15693_3_UID_SIZE);
    view_dispatcher_switch_to_view(instance->view_dispatcher, NfcMagicAppViewByteInput);
}

bool nfc_magic_scene_iso15693_write_input_on_event(void* context, SceneManagerEvent event) {
    NfcMagicApp* instance = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcMagicAppCustomEventByteInputDone) {
            // Every ISO15693 UID must begin with 0xE0; enforce it before writing (proxmark rejects
            // a non-E0 UID outright, and the byte editor lets the user change byte 0 freely).
            instance->iso15693_target_uid[0] = 0xE0;
            scene_manager_next_scene(instance->scene_manager, NfcMagicSceneIso15693WriteConfirm);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_magic_scene_iso15693_write_input_on_exit(void* context) {
    NfcMagicApp* instance = context;
    byte_input_set_result_callback(instance->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(instance->byte_input, "");
}
