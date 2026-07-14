#include "../switch_controller_app.h"

static void switch_controller_scene_macro_name_text_input_callback(void* context) {
    SwitchControllerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void switch_controller_scene_on_enter_macro_name(void* context) {
    SwitchControllerApp* app = context;

    // Clear name buffer
    memset(app->macro_name_buffer, 0, MACRO_NAME_MAX_LEN);

    text_input_set_header_text(app->text_input, "Enter Macro Name");
    text_input_set_result_callback(
        app->text_input,
        switch_controller_scene_macro_name_text_input_callback,
        app,
        app->macro_name_buffer,
        MACRO_NAME_MAX_LEN - 1,
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewTextInput);
}

bool switch_controller_scene_on_event_macro_name(void* context, SceneManagerEvent event) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Name entered, proceed to recording
        scene_manager_next_scene(app->scene_manager, SwitchControllerSceneRecording);
        consumed = true;
    }

    return consumed;
}

void switch_controller_scene_on_exit_macro_name(void* context) {
    SwitchControllerApp* app = context;
    text_input_reset(app->text_input);
}
