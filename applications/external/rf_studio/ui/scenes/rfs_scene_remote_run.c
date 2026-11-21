#include "../../rf_studio_i.h"

void rfs_scene_remote_run_on_enter(void* context) {
    RfStudioApp* app = context;
    rfs_remote_view_set_remote(app->remote_view, &app->remote, &app->signals, app->button_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, RfsViewRemote);
}

bool rfs_scene_remote_run_on_event(void* context, SceneManagerEvent event) {
    RfStudioApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        /* Back during a transmission cancels it instead of leaving (TX-009). */
        if(rfs_tx_is_running(app->tx)) {
            rfs_tx_cancel(app->tx);
            return true;
        }
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case RfsEventRemoteSend: {
        app->button_index = rfs_remote_view_get_selected(app->remote_view);
        if(app->remote.count == 0) return true;
        const RfsButton* button = &app->remote.buttons[app->button_index];
        FuriString* status = furi_string_alloc();
        bool started =
            rfs_app_send_signal(app, button->signal_id, rfs_app_remote_radio_mode(app), status);
        rfs_remote_view_set_status(
            app->remote_view,
            furi_string_get_cstr(status),
            started ? RfsStatusBusy : RfsStatusError);
        if(!started) notification_message(app->notifications, &sequence_error);
        furi_string_free(status);
        return true;
    }
    case RfsEventRemoteMenu:
        app->button_index = rfs_remote_view_get_selected(app->remote_view);
        scene_manager_set_scene_state(app->scene_manager, RfsSceneRemoteMenu, 0);
        scene_manager_next_scene(app->scene_manager, RfsSceneRemoteMenu);
        return true;
    case RfsEventTxDone: {
        char text[32];
        rfs_tx_outcome_text(app->tx, text, sizeof(text));
        RfsTxState state = rfs_tx_state(app->tx);
        rfs_remote_view_set_status(
            app->remote_view,
            text,
            state == RfsTxStateCompleted ? RfsStatusOk :
            state == RfsTxStateFailed    ? RfsStatusError :
                                           RfsStatusNone);
        if(state == RfsTxStateFailed) notification_message(app->notifications, &sequence_error);
        return true;
    }
    default:
        return false;
    }
}

void rfs_scene_remote_run_on_exit(void* context) {
    RfStudioApp* app = context;
    app->button_index = rfs_remote_view_get_selected(app->remote_view);
}
