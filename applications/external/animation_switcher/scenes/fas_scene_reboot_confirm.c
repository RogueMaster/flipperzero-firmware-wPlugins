#include "../animation_switcher.h"
#include "fas_scene.h"

static void fas_reboot_confirm_cb(DialogExResult result, void* context) {
    FasApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtRebootYes);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtRebootNo);
    }
}

void fas_scene_reboot_confirm_on_enter(void* context) {
    FasApp* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Playlist Applied!", 64, 8, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        app->dialog_ex,
        "Reboot now to load\nthe new animations?",
        64,
        30,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Later");
    dialog_ex_set_right_button_text(app->dialog_ex, "Reboot");
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, fas_reboot_confirm_cb);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewDialogEx);
}

bool fas_scene_reboot_confirm_on_event(void* context, SceneManagerEvent event) {
    FasApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasEvtRebootYes:
            /* Hard reset — the Flipper will restart and pick up the new manifest */
            furi_hal_power_reset();
            consumed = true;
            break;

        case FasEvtRebootNo:
            /* Go back to main menu without rebooting */
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FasSceneMainMenu);
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_reboot_confirm_on_exit(void* context) {
    FasApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
