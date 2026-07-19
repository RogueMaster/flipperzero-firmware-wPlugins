#include "../trident_i.h"
#include <stdio.h>
#include <string.h>

static void trident_scene_send_result(void* context) {
    TridentApp* app = context;
    if(app->input_buf[0]) {
        char line[TRIDENT_CMD_MAX];
        snprintf(line, sizeof(line), "%.62s\n", app->input_buf);
        trident_link_ensure(app);
        trident_link_send(app, line);
    }
    // Back to the console (which is still showing the live stream).
    scene_manager_previous_scene(app->scene_manager);
}

void trident_scene_send_on_enter(void* context) {
    TridentApp* app = context;
    TextInput* ti = app->text_input;

    app->input_buf[0] = '\0';
    text_input_reset(ti);
    text_input_set_header_text(ti, "Marauder command");
    text_input_set_result_callback(
        ti, trident_scene_send_result, app, app->input_buf, sizeof(app->input_buf), true);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewTextInput);
}

bool trident_scene_send_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void trident_scene_send_on_exit(void* context) {
    TridentApp* app = context;
    text_input_reset(app->text_input);
}
