#include "../trident_i.h"
#include <stdio.h>
#include <string.h>

// Generic "type a value" prompt. trident_prompt() stages a command prefix, a
// header, and an optional follow-up command; on commit we send "<prefix><value>"
// and (if set) drop into the console running the follow-up. Navigation is
// deferred to on_event because resetting the text input from inside its own
// result callback is unsafe.
static void trident_scene_input_result(void* context) {
    TridentApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, TridentCustomEventInputDone);
}

void trident_scene_input_on_enter(void* context) {
    TridentApp* app = context;
    TextInput* ti = app->text_input;

    app->input_buf[0] = '\0';
    text_input_reset(ti);
    text_input_set_header_text(ti, app->input_header);
    text_input_set_result_callback(
        ti, trident_scene_input_result, app, app->input_buf, sizeof(app->input_buf), true);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewTextInput);
}

bool trident_scene_input_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == TridentCustomEventInputDone) {
        if(app->input_buf[0]) {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "%.32s%.80s\n", app->input_prefix, app->input_buf);
            trident_link_ensure(app);
            trident_link_send(app, cmd);
        }

        // pop the input scene, then optionally open the console over the parent menu
        scene_manager_previous_scene(app->scene_manager);
        if(app->input_after_cmd[0]) {
            trident_launch(app, app->input_after_title, app->input_after_cmd, false);
        }
        return true;
    }
    return false;
}

void trident_scene_input_on_exit(void* context) {
    TridentApp* app = context;
    text_input_reset(app->text_input);
}
