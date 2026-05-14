#include "../animation_switcher.h"
#include "fas_scene.h"

static void fas_overwrite_confirm_cb(DialogExResult result, void* context) {
    FasApp* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtOverwriteYes);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtOverwriteNo);
    }
}

void fas_scene_overwrite_confirm_on_enter(void* context) {
    FasApp* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(
        app->dialog_ex, "Playlist Exists", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        app->dialog_ex,
        app->text_input_buffer,
        64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex,  "No");
    dialog_ex_set_right_button_text(app->dialog_ex, "Overwrite");
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, fas_overwrite_confirm_cb);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewDialogEx);
}

bool fas_scene_overwrite_confirm_on_event(void* context, SceneManagerEvent event) {
    FasApp* app      = context;
    bool    consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case FasEvtOverwriteYes:
            if(app->import_mode) {
                fas_import_manifest(app, app->text_input_buffer);
            } else {
                fas_save_playlist(app, app->text_input_buffer);
                /* Reset Create state so the next entry reloads fresh. */
                app->animation_count         = 0;
                app->returning_from_settings = false;
            }
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FasSceneMainMenu);
            consumed = true;
            break;

        case FasEvtOverwriteNo:
            /* Back to playlist_name.  The scene state set before the
             * forward transition makes on_enter preserve the buffer. */
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        default:
            break;
        }
    }
    return consumed;
}

void fas_scene_overwrite_confirm_on_exit(void* context) {
    FasApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}
