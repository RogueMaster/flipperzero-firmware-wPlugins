#include "../nfc_share_app.h"

void nfc_share_scene_file_browser_on_enter(void* context) {
    NfcShareApp* app = context;

    // result_path keeps the previous selection for the whole app session, so
    // the browser reopens in the same folder with that file highlighted
    // (selected_file_path is overwritten by the selection callback).
    file_browser_start(app->file_browser, app->result_path);

    // Show file browser view
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcShareViewIdFileBrowser);
}

bool nfc_share_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    if(!context) {
        return false;
    }

    NfcShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Handle file selection event
        if(event.event == 1) {
            // After selecting a file, switch to show file information
            scene_manager_next_scene(app->scene_manager, NfcShareSceneShowFile);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Handle Back button - return to main menu
        if(app->scene_manager) {
            scene_manager_previous_scene(app->scene_manager);
        }
        consumed = true;
    }

    return consumed;
}

void nfc_share_scene_file_browser_on_exit(void* context) {
    NfcShareApp* app = context;
    // Each file_browser_start allocates a new browser worker; stop it here so
    // repeated scene entries do not leak worker threads.
    file_browser_stop(app->file_browser);
}
