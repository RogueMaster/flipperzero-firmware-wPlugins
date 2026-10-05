#include "../k2_rfid_app.h"
#include "k2_scene.h"

static void k2_scene_text_input_callback(void* context) {
    K2RfidApp* app = context;
    scene_manager_previous_scene(app->scene_manager);
}

void k2_scene_text_input_on_enter(void* context) {
    K2RfidApp* app = context;
    TextInput* text_input = app->text_input;

    text_input_reset(text_input);
    text_input_set_header_text(text_input, app->text_input_header ? app->text_input_header : "Enter text");
    text_input_set_result_callback(
        text_input,
        k2_scene_text_input_callback,
        app,
        app->text_input_buf,
        sizeof(app->text_input_buf),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, K2ViewTextInput);
}

bool k2_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void k2_scene_text_input_on_exit(void* context) {
    K2RfidApp* app = context;
    text_input_reset(app->text_input);
}
