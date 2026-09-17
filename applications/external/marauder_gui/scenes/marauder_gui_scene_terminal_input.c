#include "../marauder_gui_app_i.h"

enum {
    TerminalInputEventDone,
};

static void marauder_gui_scene_terminal_input_callback(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TerminalInputEventDone);
}

void marauder_gui_scene_terminal_input_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_text_input_set_header_text(
        app->text_input, marauder_gui_text(app, "Marauder komutu", "Marauder command"));
    marauder_text_input_set_result_callback(
        app->text_input,
        marauder_gui_scene_terminal_input_callback,
        app,
        app->terminal_cmd,
        sizeof(app->terminal_cmd),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewTextInput);
}

bool marauder_gui_scene_terminal_input_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == TerminalInputEventDone) {
        if(app->terminal_cmd[0] != '\0') {
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneTerminalOutput);
        }
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_terminal_input_on_exit(void* context) {
    MarauderGuiApp* app = context;
    marauder_text_input_reset(app->text_input);
}
