#include "../breach_map_i.h"

static void breach_map_scene_pin_callback(void* context) {
    BreachMapApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, RECON_EVENT_TEXT_DONE);
}

static void breach_map_scene_pin_setup(BreachMapApp* app) {
    memset(app->text_buf, 0, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter PIN to unlock");
    text_input_set_result_callback(
        app->text_input, breach_map_scene_pin_callback, app, app->text_buf, 16, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewTextInput);
}

void breach_map_scene_pin_on_enter(void* context) {
    breach_map_scene_pin_setup(context);
}

bool breach_map_scene_pin_on_event(void* context, SceneManagerEvent event) {
    BreachMapApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == RECON_EVENT_TEXT_DONE) {
        consumed = true;
        if(breach_pin_hash(app->text_buf) == app->pin_hash) {
            app->unlocked = true;
            notification_message(app->notifications, &sequence_success);
            scene_manager_previous_scene(app->scene_manager);
        } else {
            /* wrong PIN: clear and retry in place (do not stack scenes) */
            notification_message(app->notifications, &sequence_error);
            breach_map_scene_pin_setup(app);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* never lock the user in: Back on the lock screen exits the app */
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }
    return consumed;
}

void breach_map_scene_pin_on_exit(void* context) {
    BreachMapApp* app = context;
    text_input_reset(app->text_input);
}
