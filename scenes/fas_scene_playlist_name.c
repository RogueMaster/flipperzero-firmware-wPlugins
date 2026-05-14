#include "../animation_switcher.h"
#include "fas_scene.h"

static void fas_text_input_done_cb(void* context) {
    FasApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtPlaylistNameDone);
}

void fas_scene_playlist_name_on_enter(void* context) {
    FasApp* app = context;
    memset(app->text_input_buffer, 0, FAS_PLAYLIST_NAME_LEN);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Playlist name:");
    text_input_set_result_callback(
        app->text_input,
        fas_text_input_done_cb,
        app,
        app->text_input_buffer,
        FAS_PLAYLIST_NAME_LEN,
        /*clear_default_text=*/false);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewTextInput);
}

bool fas_scene_playlist_name_on_event(void* context, SceneManagerEvent event) {
    FasApp* app      = context;
    bool    consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
        event.event == FasEvtPlaylistNameDone) {

        if(strlen(app->text_input_buffer) > 0) {
            if(app->import_mode) {
                fas_import_manifest(app, app->text_input_buffer);
            } else {
                fas_save_playlist(app, app->text_input_buffer);
                /*
                 * Reset so the next "Create Playlist" starts completely fresh:
                 * clearing animation_count forces fas_load_animations() on re-entry.
                 */
                app->animation_count      = 0;
                app->returning_from_settings = false;
            }
        }

        /* Return to the main menu — on_exit clears import_mode. */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FasSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void fas_scene_playlist_name_on_exit(void* context) {
    FasApp* app = context;
    app->import_mode = false;
}