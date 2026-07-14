#include "../vk_thermo.h"
#include "../helpers/vk_thermo_custom_event.h"
#include "../views/vk_thermo_log_view.h"
#include <gui/modules/dialog_ex.h>

static bool showing_confirm = false;

static void vk_thermo_scene_log_callback(VkThermoCustomEvent event, void* context) {
    furi_assert(context);
    VkThermo* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

static void vk_thermo_scene_log_dialog_callback(DialogExResult result, void* context) {
    furi_assert(context);
    VkThermo* app = context;
    if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, VkThermoCustomEventDialogCancel);
    } else if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, VkThermoCustomEventDialogConfirm);
    }
}

void vk_thermo_scene_log_on_enter(void* context) {
    furi_assert(context);
    VkThermo* app = context;

    showing_confirm = false;

    // Set up log view
    vk_thermo_log_view_set_callback(app->log_view, vk_thermo_scene_log_callback, app);
    vk_thermo_log_view_set_log(app->log_view, &app->log);
    vk_thermo_log_view_set_temp_unit(app->log_view, app->temp_unit);

    view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdLog);
}

bool vk_thermo_scene_log_on_event(void* context, SceneManagerEvent event) {
    VkThermo* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case VkThermoCustomEventLogBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        case VkThermoCustomEventLogSettings:
            scene_manager_next_scene(app->scene_manager, VkThermoSceneSettings);
            consumed = true;
            break;

        case VkThermoCustomEventLogClear:
            // Show confirmation dialog
            showing_confirm = true;
            dialog_ex_set_header(app->dialog, "Clear History?", 64, 12, AlignCenter, AlignCenter);
            dialog_ex_set_text(
                app->dialog, "All readings will\nbe deleted.", 64, 32, AlignCenter, AlignCenter);
            dialog_ex_set_left_button_text(app->dialog, "Cancel");
            dialog_ex_set_right_button_text(app->dialog, "Clear");
            dialog_ex_set_result_callback(app->dialog, vk_thermo_scene_log_dialog_callback);
            dialog_ex_set_context(app->dialog, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdDialog);
            consumed = true;
            break;

        case VkThermoCustomEventDialogConfirm:
            // User confirmed clear
            showing_confirm = false;
            vk_thermo_log_clear(&app->log);
            vk_thermo_log_view_set_log(app->log_view, &app->log);
            view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdLog);
            consumed = true;
            break;

        case VkThermoCustomEventDialogCancel:
            // User cancelled clear
            showing_confirm = false;
            view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdLog);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(showing_confirm) {
            // Back from dialog returns to log
            showing_confirm = false;
            view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdLog);
            consumed = true;
        } else {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void vk_thermo_scene_log_on_exit(void* context) {
    VkThermo* app = context;
    showing_confirm = false;
    dialog_ex_reset(app->dialog);
}
