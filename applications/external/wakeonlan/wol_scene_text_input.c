#include "wol_flipper.h"

static void wol_scene_text_input_result(void* context) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void wol_scene_text_input_on_enter(void* context) {
    WolApp* app = context;

    const char* header = "";
    size_t max_len = sizeof(app->text_buf);

    switch(app->text_field) {
    case WolTextFieldName:
        header = "Target name";
        max_len = WOL_NAME_LEN;
        break;
    case WolTextFieldIp:
        header = "Broadcast, not host IP";
        max_len = WOL_IP_LEN;
        break;
    case WolTextFieldSsid:
        header = "Wi-Fi SSID";
        max_len = WOL_SSID_LEN;
        break;
    case WolTextFieldPassword:
        header = "Wi-Fi password";
        max_len = WOL_PASS_LEN;
        break;
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, wol_scene_text_input_result, app, app->text_buf, max_len, false);

    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewTextInput);
}

bool wol_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(app->text_field) {
    case WolTextFieldName:
        wol_strcpy(app->edit.name, WOL_NAME_LEN, app->text_buf);
        break;
    case WolTextFieldIp:
        wol_strcpy(app->edit.ip, WOL_IP_LEN, app->text_buf);
        break;
    // networks are committed by the edit scene, not here
    case WolTextFieldSsid:
        wol_strcpy(app->edit_network.ssid, WOL_SSID_LEN, app->text_buf);
        break;
    case WolTextFieldPassword:
        wol_strcpy(app->edit_network.pass, WOL_PASS_LEN, app->text_buf);
        break;
    }

    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void wol_scene_text_input_on_exit(void* context) {
    WolApp* app = context;
    text_input_reset(app->text_input);
}
