#include "wol_flipper.h"

static void wol_scene_byte_input_result(void* context) {
    WolApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void wol_scene_byte_input_on_enter(void* context) {
    WolApp* app = context;

    byte_input_set_header_text(app->byte_input, "Enter target MAC");
    byte_input_set_result_callback(
        app->byte_input, wol_scene_byte_input_result, NULL, app, app->edit.mac, 6);

    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewByteInput);
}

bool wol_scene_byte_input_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void wol_scene_byte_input_on_exit(void* context) {
    WolApp* app = context;
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
