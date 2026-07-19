#include "../gauntlet_i.h"
#include <string.h>

static void gauntlet_toolinput_cb(void* context) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GauntletEventToolCommit);
}

void gauntlet_scene_toolinput_on_enter(void* context) {
    GauntletApp* app = context;
    TextInput* ti = app->text_input;

    app->text_buf[0] = '\0';
    text_input_reset(ti);
    text_input_set_header_text(ti, "Text to decode");
    text_input_set_result_callback(
        ti, gauntlet_toolinput_cb, app, app->text_buf, sizeof(app->text_buf), false);

    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewTextInput);
}

bool gauntlet_scene_toolinput_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == GauntletEventToolCommit) {
        strncpy(app->tool_src, app->text_buf, sizeof(app->tool_src) - 1);
        app->tool_src[sizeof(app->tool_src) - 1] = '\0';
        app->tool_from_challenge = false;
        scene_manager_next_scene(app->scene_manager, GauntletSceneToolkit);
        consumed = true;
    }
    return consumed;
}

void gauntlet_scene_toolinput_on_exit(void* context) {
    GauntletApp* app = context;
    text_input_reset(app->text_input);
}
