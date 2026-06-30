#include "../nfc_maker.h"

enum ByteInputResult {
    ByteInputResultOk,
};

static void nfc_maker_scene_bluetooth_byte_input_callback(void* context) {
    NfcMaker* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, ByteInputResultOk);
}

static void reverse_mac_addr(uint8_t mac_addr[GAP_MAC_ADDR_SIZE]) {
    uint8_t tmp;
    for(size_t i = 0; i < GAP_MAC_ADDR_SIZE / 2; i++) {
        tmp = mac_addr[i];
        mac_addr[i] = mac_addr[GAP_MAC_ADDR_SIZE - 1 - i];
        mac_addr[GAP_MAC_ADDR_SIZE - 1 - i] = tmp;
    }
}

void nfc_maker_scene_bluetooth_on_enter(void* context) {
    NfcMaker* app = context;
    ByteInput* byte_input = app->byte_input;

    byte_input_set_header_text(byte_input, "Enter Bluetooth MAC:");

    for(size_t i = 0; i < sizeof(app->mac_buf); i++) {
        app->mac_buf[i] = 0x42;
    }

    byte_input_set_result_callback(
        byte_input,
        nfc_maker_scene_bluetooth_byte_input_callback,
        NULL,
        app,
        app->mac_buf,
        sizeof(app->mac_buf));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMakerViewByteInput);
}

bool nfc_maker_scene_bluetooth_on_event(void* context, SceneManagerEvent event) {
    NfcMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ByteInputResultOk:
            reverse_mac_addr(app->mac_buf);
            scene_manager_next_scene(app->scene_manager, NfcMakerSceneSaveGenerate);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void nfc_maker_scene_bluetooth_on_exit(void* context) {
    NfcMaker* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
