#include "../animation_switcher.h"
#include "fas_scene.h"

static void fas_text_input_done_cb(void* context) {
    FasApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FasEvtPlaylistNameDone);
}

static bool fas_playlist_name_validator(
    const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(text[0] == '\0') {
        furi_string_set(error, "Name cannot\nbe empty");
        return false;
    }
    if(text[0] == '.') {
        furi_string_set(error, "Name cannot\nstart with '.'");
        return false;
    }
    for(const char* p = text; *p; p++) {
        if(*p == '/' || *p == '\\' || *p == ':') {
            furi_string_set(error, "Name contains\nillegal character");
            return false;
        }
    }
    return true;
}

void fas_scene_playlist_name_on_enter(void* context) {
    FasApp* app = context;

    /* Re-entry from OverwriteConfirm preserves the buffer so the user can
     * tweak the existing name instead of typing it again. */
    bool reentering =
        scene_manager_get_scene_state(app->scene_manager, FasScenePlaylistName) != 0;
    scene_manager_set_scene_state(app->scene_manager, FasScenePlaylistName, 0);

    if(!reentering) {
        memset(app->text_input_buffer, 0, FAS_PLAYLIST_NAME_LEN);
        text_input_reset(app->text_input);
    }

    text_input_set_header_text(app->text_input, "Playlist name:");
    text_input_set_result_callback(
        app->text_input,
        fas_text_input_done_cb,
        app,
        app->text_input_buffer,
        FAS_PLAYLIST_NAME_LEN,
        /*clear_default_text=*/!reentering);
    text_input_set_validator(
        app->text_input, fas_playlist_name_validator, NULL);

    view_dispatcher_switch_to_view(app->view_dispatcher, FasViewTextInput);
}

bool fas_scene_playlist_name_on_event(void* context, SceneManagerEvent event) {
    FasApp* app      = context;
    bool    consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
        event.event == FasEvtPlaylistNameDone) {

        if(strlen(app->text_input_buffer) > 0) {
            if(fas_playlist_exists(app, app->text_input_buffer)) {
                /* Defer the save until the user confirms the overwrite.
                 * Setting state=1 makes on_enter preserve the buffer if the
                 * user backs out of the confirmation. */
                scene_manager_set_scene_state(
                    app->scene_manager, FasScenePlaylistName, 1);
                scene_manager_next_scene(
                    app->scene_manager, FasSceneOverwriteConfirm);
                return true;
            }

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

        /* Return to the main menu.  main_menu on_enter clears import_mode. */
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FasSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void fas_scene_playlist_name_on_exit(void* context) {
    UNUSED(context);
}
