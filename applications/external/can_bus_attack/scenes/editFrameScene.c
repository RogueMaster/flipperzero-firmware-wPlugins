#include "../app_user.h"

//
// --- Attack DoS Frame Editing ---
//
static void attack_dos_edit_callback(void* context) {
    App* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void app_scene_attack_dos_edit_frame_on_enter(void* context) {
    App* app = context;

    byte_input_set_header_text(app->input_byte_value, "Edit Payload (DoS)");
    byte_input_set_result_callback(
        app->input_byte_value,
        attack_dos_edit_callback,
        NULL,
        app,
        app->frame_to_send->buffer,
        app->frame_to_send->data_lenght);

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_attack_dos_edit_frame_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_attack_dos_edit_frame_on_exit(void* context) {
    UNUSED(context);
}

//
// --- Modify Attack Frame Editing ---
//
static void modify_attack_edit_callback(void* context) {
    App* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void app_scene_modify_attack_edit_frame_on_enter(void* context) {
    App* app = context;

    byte_input_set_header_text(app->input_byte_value, "Edit Payload (Modify)");
    byte_input_set_result_callback(
        app->input_byte_value,
        modify_attack_edit_callback,
        NULL,
        app,
        app->modify_frame->buffer,
        app->modify_frame->data_lenght);

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_modify_attack_edit_frame_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_modify_attack_edit_frame_on_exit(void* context) {
    UNUSED(context);
}
