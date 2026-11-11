#include "../cfw_app.h"

enum ByteInputResult {
    ByteInputResultOk,
};

void cfw_app_scene_misc_spoof_serial_byte_input_callback(void* context) {
    CFWApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, ByteInputResultOk);
}

void cfw_app_scene_misc_spoof_serial_on_enter(void* context) {
    CFWApp* app = context;
    ByteInput* byte_input = app->byte_input;

    // Note: the serial feeds furi_hal_version_uid(), which nfc_login uses to
    // derive passcodes. Spoofing it invalidates saved nfc_login codes until it
    // is set back to all-00. Reversible; surfaced by the "all 00 = real" hint.
    byte_input_set_header_text(byte_input, "Serial/UID (all 00 = real)");

    byte_input_set_result_callback(
        byte_input,
        cfw_app_scene_misc_spoof_serial_byte_input_callback,
        NULL,
        app,
        app->spoof_serial,
        sizeof(app->spoof_serial));

    view_dispatcher_switch_to_view(app->view_dispatcher, CFWAppViewByteInput);
}

bool cfw_app_scene_misc_spoof_serial_on_event(void* context, SceneManagerEvent event) {
    CFWApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case ByteInputResultOk:
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

void cfw_app_scene_misc_spoof_serial_on_exit(void* context) {
    CFWApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
