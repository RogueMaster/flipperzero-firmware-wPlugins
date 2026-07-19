#include "../trident_i.h"
#include <stdio.h>
#include <string.h>

// Defer the actual work out of the text-input callback: navigating (which resets
// the text input) from inside its own result handler is unsafe, so we bounce
// through a custom event handled in on_event.
static void trident_scene_select_result(void* context) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TridentCustomEventSelectApplied);
}

void trident_scene_select_on_enter(void* context) {
    TridentApp* app = context;
    TextInput* ti = app->text_input;

    app->input_buf[0] = '\0';
    text_input_reset(ti);
    text_input_set_header_text(
        ti, app->select_kind == 's' ? "Station index (e.g. 0)" : "AP index (e.g. 0)");
    text_input_set_result_callback(
        ti, trident_scene_select_result, app, app->input_buf, sizeof(app->input_buf), true);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewTextInput);
}

bool trident_scene_select_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TridentCustomEventSelectApplied) {
        const char* idx = app->input_buf[0] ? app->input_buf : "0";
        char cmd[TRIDENT_CMD_MAX];
        // MARAUDER_FMT_SELECT is "select -%c %s"; bound the index to keep it in-buffer
        snprintf(cmd, sizeof(cmd), "select -%c %.40s\n", app->select_kind, idx);

        trident_link_ensure(app);
        trident_link_send(app, cmd);

        const char* list_cmd =
            (app->select_kind == 's') ? MARAUDER_CMD_LIST_STA : MARAUDER_CMD_LIST_AP;
        const char* title = (app->select_kind == 's') ? "Station List" : "AP List";

        // Drop the input scene, then open the console over the target menu so a
        // single Back returns cleanly to Targets.
        scene_manager_previous_scene(app->scene_manager);
        trident_launch(app, title, list_cmd, false);
        return true;
    }
    return false;
}

void trident_scene_select_on_exit(void* context) {
    TridentApp* app = context;
    text_input_reset(app->text_input);
}
