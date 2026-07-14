#include "app_user.h"

#define TAG "SLCAN OPTIONS SCENE"

uint32_t select_index_SLCAN_options = 0;

void submenu_callback_SLCAN_options(void* context, uint32_t index) {
    App* app = context;

    scene_manager_handle_custom_event(app->scene_manager, index);
}

void app_scene_transmit_options_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "TRANSMIT OPTIONS");

    submenu_add_item(
        app->submenu, "Send log", SLCAN_SEND_LOG, submenu_callback_SLCAN_options, app);
    submenu_add_item(
        app->submenu, "Send frames", SLCAN_SEND_FRAME, submenu_callback_SLCAN_options, app);

    submenu_set_selected_item(app->submenu, select_index_SLCAN_options);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuLogView);
}

bool app_scene_transmit_options_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case SLCAN_SEND_FRAME:
            *app->can_send_frame = true;
            scene_manager_next_scene(app->scene_manager, app_scene_dialog_scene);
            break;
        case SLCAN_SEND_LOG:
            DialogMessage* message = dialog_message_alloc();

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            SLCAN_send_log(app->storage, app->file_active->path, app->send_timestamp);

            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuLogView);

            dialog_message_set_icon(message, &I_EC48x26, 40, 1);
            dialog_message_set_header(
                message,
                "Finished transmitting\nthe log via SLCAN",
                64,
                45,
                AlignCenter,
                AlignCenter);

            dialog_message_show(app->dialogs, message);

            dialog_message_free(message);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return consumed;
}

void app_scene_transmit_options_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
