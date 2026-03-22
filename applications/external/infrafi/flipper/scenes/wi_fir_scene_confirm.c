#include "../wi_fir.h"
#include "../wfr_storage.h"
#include <stdio.h>

static const char* security_labels[] = {"Open", "WPA", "WEP", "SAE"};

static void wi_fir_scene_confirm_delete_callback(DialogExResult result, void* context) {
    WiFirApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventDeleteConfirmed);
    } else {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventDeleteCancelled);
    }
}

static void wi_fir_scene_confirm_callback(DialogExResult result, void* context) {
    WiFirApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmSend);
    } else if(result == DialogExResultLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmBack);
    } else if(result == DialogExResultCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventConfirmDelete);
    }
}

void wi_fir_scene_confirm_on_enter(void* context) {
    WiFirApp* app = context;

    const char* sec = (app->security_type < 4) ? security_labels[app->security_type] : "?";

    snprintf(
        app->confirm_text,
        sizeof(app->confirm_text),
        "SSID: %s\nSec: %s\nPass: %s",
        app->ssid,
        sec,
        (app->password[0] != '\0') ? "****" : "(none)");

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Confirm", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, app->confirm_text, 0, 16, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(app->dialog_ex, "Back");
    dialog_ex_set_right_button_text(app->dialog_ex, "Send");

    /* Show delete button if coming from saved networks */
    if(app->selected_saved_file[0] != '\0') {
        dialog_ex_set_center_button_text(app->dialog_ex, "Del");
    }
    dialog_ex_set_result_callback(app->dialog_ex, wi_fir_scene_confirm_callback);
    dialog_ex_set_context(app->dialog_ex, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewDialogEx);
}

bool wi_fir_scene_confirm_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventConfirmSend) {
            scene_manager_next_scene(app->scene_manager, WiFirSceneTransmit);
            consumed = true;
        } else if(event.event == WiFirCustomEventConfirmBack) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == WiFirCustomEventConfirmDelete) {
            if(app->selected_saved_file[0] != '\0') {
                /* Show "are you sure?" confirmation */
                dialog_ex_reset(app->dialog_ex);
                dialog_ex_set_header(
                    app->dialog_ex, "Delete?", 64, 0, AlignCenter, AlignTop);
                snprintf(
                    app->confirm_text,
                    sizeof(app->confirm_text),
                    "Remove \"%s\"\nfrom saved networks?",
                    app->ssid);
                dialog_ex_set_text(
                    app->dialog_ex, app->confirm_text, 64, 28, AlignCenter, AlignCenter);
                dialog_ex_set_left_button_text(app->dialog_ex, "No");
                dialog_ex_set_right_button_text(app->dialog_ex, "Yes");
                dialog_ex_set_center_button_text(app->dialog_ex, NULL);
                dialog_ex_set_result_callback(
                    app->dialog_ex, wi_fir_scene_confirm_delete_callback);
                dialog_ex_set_context(app->dialog_ex, app);
                /* Stay on same view — dialog_ex is already showing */
            }
            consumed = true;
        } else if(event.event == WiFirCustomEventDeleteConfirmed) {
            wfr_storage_delete(app->storage, app->selected_saved_file);
            app->selected_saved_file[0] = '\0';
            /* Go back to saved list */
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == WiFirCustomEventDeleteCancelled) {
            /* Restore the original confirm dialog */
            wi_fir_scene_confirm_on_enter(app);
            consumed = true;
        }
    }

    return consumed;
}

void wi_fir_scene_confirm_on_exit(void* context) {
    WiFirApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
