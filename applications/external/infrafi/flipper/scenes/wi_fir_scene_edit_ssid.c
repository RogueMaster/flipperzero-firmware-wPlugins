#include "../wi_fir.h"

static void wi_fir_scene_edit_ssid_callback(void* context) {
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventTextInputDone);
}

void wi_fir_scene_edit_ssid_on_enter(void* context) {
    WiFirApp* app = context;

    /* Pre-fill with existing SSID if any */
    strncpy(app->text_input_buf, app->ssid, sizeof(app->text_input_buf) - 1);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter SSID:");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_result_callback(
        app->text_input,
        wi_fir_scene_edit_ssid_callback,
        app,
        app->text_input_buf,
        WFR_SSID_MAX_LEN + 1,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewTextInput);
}

bool wi_fir_scene_edit_ssid_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventTextInputDone) {
            /* Save SSID and move to password entry */
            strncpy(app->ssid, app->text_input_buf, sizeof(app->ssid) - 1);
            app->ssid[sizeof(app->ssid) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, WiFirSceneEditPassword);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_edit_ssid_on_exit(void* context) {
    WiFirApp* app = context;
    text_input_reset(app->text_input);
}
