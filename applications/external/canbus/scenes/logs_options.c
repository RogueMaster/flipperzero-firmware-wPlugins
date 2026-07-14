#include "app_user.h"

#define TAG "OPTIONS LOGS"

char logs_options_index = 0;

void submenu_callback_logs_options(void* context, uint32_t index) {
    App* app = context;

    scene_manager_handle_custom_event(app->scene_manager, index);
    logs_options_index = index;
}

void app_scene_logs_options_on_enter(void* context) {
    App* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "LOGS OPTIONS");

    submenu_add_item(
        app->submenu, "View frames from the log", VIEW_LOG, submenu_callback_logs_options, app);
    submenu_add_item(
        app->submenu, "Export log as CSV", EXPORT_LOG, submenu_callback_logs_options, app);
    submenu_add_item(
        app->submenu, "Transmit log to SLCAN", TRANSMIT_LOG, submenu_callback_logs_options, app);

    submenu_set_selected_item(app->submenu, logs_options_index);

    view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuLogView);
}

bool app_scene_logs_options_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case VIEW_LOG:
            scene_manager_next_scene(app->scene_manager, app_scene_dialog_scene);
            consumed = true;
            break;
        case EXPORT_LOG:
            FuriString* out_path = furi_string_alloc();
            DialogMessage* message = dialog_message_alloc();

            view_dispatcher_switch_to_view(app->view_dispatcher, LoadingView);

            export_log_as_csv(app->storage, app->file_active->path, out_path);

            view_dispatcher_switch_to_view(app->view_dispatcher, SubmenuLogView);

            dialog_message_set_icon(message, &I_EC48x26, 40, 1);
            dialog_message_set_header(
                message, "Log exported to:", 64, 40, AlignCenter, AlignCenter);
            dialog_message_set_text(
                message, furi_string_get_cstr(out_path), 64, 55, AlignCenter, AlignCenter);

            dialog_message_show(app->dialogs, message);

            dialog_message_free(message);
            furi_string_free(out_path);
            consumed = true;
            break;
        case TRANSMIT_LOG:
            scene_manager_next_scene(app->scene_manager, app_scene_send_timestamp_scene);
            consumed = true;
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

void app_scene_logs_options_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
