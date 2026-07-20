#include "../glitch_trigger_i.h"

/* Text-entry for a new profile name. On OK we save the current params under the
 * typed name and pop back to the profiles list. */

static void glitch_scene_profilename_cb(void* context) {
    GlitchApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void glitch_scene_profilename_on_enter(void* context) {
    GlitchApp* app = context;
    TextInput* ti = app->text_input;

    text_input_reset(ti);
    text_input_set_header_text(ti, "Profile name");
    text_input_set_minimum_length(ti, 1);
    text_input_set_result_callback(
        ti,
        glitch_scene_profilename_cb,
        app,
        app->name_buf,
        GLITCH_PROFILE_NAME_MAX,
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewTextInput);
}

bool glitch_scene_profilename_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        bool ok = glitch_storage_save_profile(app->name_buf, &app->params);
        glitch_notify_hit(app); // brief confirmation blip
        UNUSED(ok);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void glitch_scene_profilename_on_exit(void* context) {
    GlitchApp* app = context;
    text_input_reset(app->text_input);
}
