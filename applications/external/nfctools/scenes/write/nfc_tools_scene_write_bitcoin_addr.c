#include "../../include/nfc_tools_i.h"

static void nfc_tools_scene_write_bitcoin_addr_callback(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void nfc_tools_scene_write_bitcoin_addr_on_enter(void* context) {
    NfcToolsApp* app = context;
    Keyboard* input = app->keyboard;
    keyboard_set_header_text(input, NTS_INPUT_BTC_ADDRESS);
    keyboard_set_result_callback(
        input,
        nfc_tools_scene_write_bitcoin_addr_callback,
        app,
        app->ndef_buf1,
        sizeof(app->ndef_buf1),
        false);
    keyboard_set_minimum_length(input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewKeyboard);
}

bool nfc_tools_scene_write_bitcoin_addr_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdWriteBitcoinAmount);
        consumed = true;
    }
    return consumed;
}

void nfc_tools_scene_write_bitcoin_addr_on_exit(void* context) {
    NfcToolsApp* app = context;
    keyboard_reset(app->keyboard);
}
