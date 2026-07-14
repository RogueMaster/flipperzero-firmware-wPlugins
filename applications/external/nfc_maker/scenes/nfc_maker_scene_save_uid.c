#include "../nfc_maker.h"

enum ByteInputResult {
    ByteInputResultOk,
};

static void nfc_maker_scene_save_uid_byte_input_callback(void* context) {
    NfcMaker* app = context;

    size_t uid_len;
    nfc_device_get_uid(app->nfc_device, &uid_len);
    bool uid_valid = nfc_device_set_uid(app->nfc_device, app->uid_buf, uid_len);

    if(uid_valid) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ByteInputResultOk);
    } else {
        byte_input_set_header_text(app->byte_input, "Invalid UID!");
    }
}

static void nfc_maker_scene_save_uid_byte_input_changed(void* context) {
    NfcMaker* app = context;

    byte_input_set_header_text(app->byte_input, "Change UID:");
}

void nfc_maker_scene_save_uid_on_enter(void* context) {
    NfcMaker* app = context;
    ByteInput* byte_input = app->byte_input;

    byte_input_set_header_text(byte_input, "(Optional) Change UID:");

    size_t uid_len;
    const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &uid_len);
    memcpy(app->uid_buf, uid, uid_len);

    byte_input_set_result_callback(
        byte_input,
        nfc_maker_scene_save_uid_byte_input_callback,
        nfc_maker_scene_save_uid_byte_input_changed,
        app,
        app->uid_buf,
        uid_len);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMakerViewByteInput);
}

bool nfc_maker_scene_save_uid_on_event(void* context, SceneManagerEvent event) {
    NfcMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ByteInputResultOk:
            scene_manager_next_scene(app->scene_manager, NfcMakerSceneSaveName);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_maker_scene_save_uid_on_exit(void* context) {
    NfcMaker* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
