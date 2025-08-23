#include "../flipper_share_app.h"

void flipper_share_scene_file_browser_on_enter(void* context) {
    FlipperShareApp* app = context;

    // Reset selected file path
    app->selected_file_path[0] = '\0';

    // Start file browser
    FuriString* path = furi_string_alloc_set_str("/ext");
    file_browser_start(app->file_browser, path);
    furi_string_free(path);

    // Show file browser view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdFileBrowser);
}

bool flipper_share_scene_file_browser_on_event(void* context, SceneManagerEvent event) {
    if(!context) {
        return false;
    }

    FlipperShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Handle file selection event
        if(event.event == 1) {
            // After selecting a file, switch to show file information
            scene_manager_next_scene(app->scene_manager, FlipperShareSceneShowFile);
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

void flipper_share_scene_file_browser_on_exit(void* context) {
    UNUSED(context);
}
