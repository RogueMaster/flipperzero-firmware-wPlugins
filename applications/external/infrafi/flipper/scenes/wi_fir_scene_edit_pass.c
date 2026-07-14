#include "../wi_fir.h"

static void wi_fir_scene_edit_password_callback(void* context) {
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventTextInputDone);
}

void wi_fir_scene_edit_password_on_enter(void* context) {
    WiFirApp* app = context;

    /* Pre-fill with existing password if any */
    strncpy(app->text_input_buf, app->password, sizeof(app->text_input_buf) - 1);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter Password:");
    /* Password can be empty for open networks, so minimum_length = 0 (default) */
    text_input_set_result_callback(
        app->text_input,
        wi_fir_scene_edit_password_callback,
        app,
        app->text_input_buf,
        WFR_PASS_MAX_LEN + 1,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewTextInput);
}

bool wi_fir_scene_edit_password_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventTextInputDone) {
            /* Save password and move to security type selection */
            strncpy(app->password, app->text_input_buf, sizeof(app->password) - 1);
            app->password[sizeof(app->password) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, WiFirSceneEditSecurity);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_edit_password_on_exit(void* context) {
    WiFirApp* app = context;
    text_input_reset(app->text_input);
}
