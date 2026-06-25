#include "../argus_i.h"
#include <stdio.h>

static void argus_scene_guard_input_cb(void* context) {
    ArgusApp* app = context;

    argus_db_set_guard(app->db, app->guard_input);

    /* tell the board which SSID we care about (it's informational on the ESP) */
    if(uart_link_is_running(app->uart)) {
        char cmd[16 + ARGUS_GUARD_INPUT_LEN];
        snprintf(cmd, sizeof(cmd), "GUARD:%s\n", app->guard_input);
        uart_link_send_command(app->uart, cmd);
    }

    scene_manager_previous_scene(app->scene_manager);
}

void argus_scene_guard_on_enter(void* context) {
    ArgusApp* app = context;
    TextInput* text_input = app->text_input;

    argus_db_get_guard(app->db, app->guard_input, sizeof(app->guard_input));

    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Your network's SSID");
    text_input_set_result_callback(
        text_input,
        argus_scene_guard_input_cb,
        app,
        app->guard_input,
        sizeof(app->guard_input),
        false); // keep the existing SSID pre-filled for editing

    view_dispatcher_switch_to_view(app->view_dispatcher, ArgusViewTextInput);
}

bool argus_scene_guard_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void argus_scene_guard_on_exit(void* context) {
    ArgusApp* app = context;
    text_input_reset(app->text_input);
}
