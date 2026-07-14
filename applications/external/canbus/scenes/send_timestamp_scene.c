#include "app_user.h"

#define TAG "SEND TIMESTAMP"

void callback_dialog_send_timestamp(DialogExResult result, void* context) {
    App* app = context;

    switch(result) {
    case DialogExResultLeft:
        *app->send_timestamp = false;
        scene_manager_next_scene(app->scene_manager, app_scene_transmit_options_scene);
        break;
    case DialogExResultRight:
        *app->send_timestamp = true;
        scene_manager_next_scene(app->scene_manager, app_scene_transmit_options_scene);
        break;
    default:
        break;
    }
}

void app_scene_send_timestamp_on_enter(void* context) {
    furi_assert(context);
    App* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, callback_dialog_send_timestamp);

    dialog_ex_set_header(
        app->dialog_ex, "Would you like to\nsend the timestamp?", 64, 27, AlignCenter, AlignCenter);

    dialog_ex_set_left_button_text(app->dialog_ex, "No");
    dialog_ex_set_right_button_text(app->dialog_ex, "Yes");

    view_dispatcher_switch_to_view(app->view_dispatcher, DialogView);
}

bool app_scene_send_timestamp_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_send_timestamp_on_exit(void* context) {
    furi_assert(context);
    App* app = context;
    *app->send_timestamp = false;
    dialog_ex_reset(app->dialog_ex);
}
