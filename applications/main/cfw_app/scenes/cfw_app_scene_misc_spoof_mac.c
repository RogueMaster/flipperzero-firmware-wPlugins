#include "../cfw_app.h"

enum ByteInputResult {
    ByteInputResultOk,
};

void cfw_app_scene_misc_spoof_mac_byte_input_callback(void* context) {
    CFWApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, ByteInputResultOk);
}

void cfw_app_scene_misc_spoof_mac_on_enter(void* context) {
    CFWApp* app = context;
    ByteInput* byte_input = app->byte_input;

    byte_input_set_header_text(byte_input, "BLE MAC (all 00 = real)");

    byte_input_set_result_callback(
        byte_input,
        cfw_app_scene_misc_spoof_mac_byte_input_callback,
        NULL,
        app,
        app->spoof_mac,
        sizeof(app->spoof_mac));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewByteInput);
}

bool cfw_app_scene_misc_spoof_mac_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ByteInputResultOk:
            // Reuse save_name as the "rewrite name.settings" flag; a reboot
            // re-applies the identity via namespoof_init().
            app->save_name = true;
            app->require_reboot = true;
            scene_manager_previous_scene(app->scene_manager);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void cfw_app_scene_misc_spoof_mac_on_exit(void* context) {
    CFWApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
