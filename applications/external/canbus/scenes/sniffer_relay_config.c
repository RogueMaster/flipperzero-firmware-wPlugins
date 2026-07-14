#include "app_user.h"

#define TAG "SNIFFER RELAY CONFIG"

void callback_dialog_sniffer_relay_config(DialogExResult result, void* context) {
    App* app = context;

    switch(result) {
    case DialogExResultLeft:
        *app->can_send_frame = false;
        scene_manager_next_scene(app->scene_manager, app_scene_sniffing_option);
        break;
    case DialogExResultRight:
        *app->can_send_frame = true;
        scene_manager_next_scene(app->scene_manager, app_scene_sniffing_option);
        break;
    default:
        break;
    }
}

void app_scene_sniffer_relay_config_on_enter(void* context) {
    furi_assert(context);
    App* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, callback_dialog_sniffer_relay_config);

    dialog_ex_set_header(
        app->dialog_ex,
        "Would you like to\nrelay the messages\nreceived via SLCAN?",
        64,
        27,
        AlignCenter,
        AlignCenter);

    dialog_ex_set_left_button_text(app->dialog_ex, "No");
    dialog_ex_set_right_button_text(app->dialog_ex, "Yes");

    view_dispatcher_switch_to_view(app->view_dispatcher, DialogView);
}

bool app_scene_sniffer_relay_config_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    bool consumed = false;

    return consumed;
}

void app_scene_sniffer_relay_config_on_exit(void* context) {
    furi_assert(context);
    App* app = context;
    dialog_ex_reset(app->dialog_ex);
}
