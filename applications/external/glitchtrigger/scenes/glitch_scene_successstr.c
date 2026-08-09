#include "../glitch_trigger_i.h"

/* Text-entry for the UART success string. The text-input writes straight into
 * params.success_str; OK just pops back to Settings. */

static void glitch_scene_successstr_cb(void* context) {
    GlitchApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void glitch_scene_successstr_on_enter(void* context) {
    GlitchApp* app = context;
    TextInput* ti = app->text_input;

    text_input_reset(ti);
    text_input_set_header_text(ti, "Success string");
    text_input_set_minimum_length(ti, 1);
    text_input_set_result_callback(
        ti,
        glitch_scene_successstr_cb,
        app,
        app->params.success_str,
        GLITCH_SUCCESS_MAX,
        false); // keep the current string as the starting text

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewTextInput);
}

bool glitch_scene_successstr_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void glitch_scene_successstr_on_exit(void* context) {
    GlitchApp* app = context;
    text_input_reset(app->text_input);
}
