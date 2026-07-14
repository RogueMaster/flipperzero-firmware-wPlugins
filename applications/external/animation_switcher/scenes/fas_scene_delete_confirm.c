#include "../animation_switcher.h"
#include "fas_scene.h"

static void fas_delete_confirm_cb(DialogExResult result, void* context) {
    FasApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtDeleteYes);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtDeleteNo);
    }
}

void fas_scene_delete_confirm_on_enter(void* context) {
    FasApp* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Delete Playlist?", 64, 10, AlignCenter, AlignCenter);

    /* Show the playlist name as the body text */
    dialog_ex_set_text(
        app->dialog_ex,
        app->playlists[app->current_playlist_index].name,
        64,
        32,
        AlignCenter,
        AlignCenter);

    dialog_ex_set_left_button_text(app->dialog_ex, "No");
    dialog_ex_set_right_button_text(app->dialog_ex, "Yes");
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, fas_delete_confirm_cb);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewDialogEx);
}

bool fas_scene_delete_confirm_on_event(void* context, SceneManagerEvent event) {
    FasApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasEvtDeleteYes:
            fas_delete_playlist(app, app->current_playlist_index);
            /* Pop back to the delete-playlist list so it reloads */
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        case FasEvtDeleteNo:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_delete_confirm_on_exit(void* context) {
    FasApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
