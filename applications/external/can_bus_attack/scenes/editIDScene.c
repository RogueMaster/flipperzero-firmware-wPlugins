#include "../app_user.h"

// --- Buffer local compartido ---
static uint8_t id_bytes[2] = {0};

//
// ------- DoS Attack ID Editing -------
//
static void attack_dos_edit_id_callback(void* context) {
    App* app = context;

    uint32_t new_id = ((uint32_t)id_bytes[0] << 8) | id_bytes[1];
    app->frame_to_send->canId = new_id;

    scene_manager_previous_scene(app->scene_manager);
}

void app_scene_attack_dos_edit_id_on_enter(void* context) {
    App* app = context;

    id_bytes[0] = (app->frame_to_send->canId >> 8) & 0xFF;
    id_bytes[1] = app->frame_to_send->canId & 0xFF;

    byte_input_set_header_text(app->input_byte_value, "Edit ID (DoS)");

    byte_input_set_result_callback(
        app->input_byte_value, attack_dos_edit_id_callback, NULL, app, id_bytes, 2);

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_attack_dos_edit_id_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_attack_dos_edit_id_on_exit(void* context) {
    UNUSED(context);
}

//
// ------- Modify Attack ID Editing -------
//
static void modify_attack_edit_id_callback(void* context) {
    App* app = context;

    uint32_t new_id = ((uint32_t)id_bytes[0] << 8) | id_bytes[1];
    app->modify_frame->canId = new_id;

    scene_manager_previous_scene(app->scene_manager);
}

void app_scene_modify_attack_edit_id_on_enter(void* context) {
    App* app = context;

    id_bytes[0] = (app->modify_frame->canId >> 8) & 0xFF;
    id_bytes[1] = app->modify_frame->canId & 0xFF;

    byte_input_set_header_text(app->input_byte_value, "Edit ID (Modify)");

    byte_input_set_result_callback(
        app->input_byte_value, modify_attack_edit_id_callback, NULL, app, id_bytes, 2);

    view_dispatcher_switch_to_view(app->view_dispatcher, InputByteView);
}

bool app_scene_modify_attack_edit_id_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void app_scene_modify_attack_edit_id_on_exit(void* context) {
    UNUSED(context);
}
