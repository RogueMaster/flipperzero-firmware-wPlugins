#include "../app_user.h"

/**
 * This scene it is only to load and get pcap paths to display
 */

void file_browser_callback(void* context) {
    App* app = (App*)context;

    scene_manager_next_scene(app->scene_manager, app_scene_read_pcap_option);
}

// Function for the testing scene on enter
void app_scene_browser_pcaps_on_enter(void* context) {
    App* app = (App*)context;

    file_browser_configure(app->file_browser, ".pcap", PATHPCAPS, true, true, NULL, true);
    file_browser_set_callback(app->file_browser, file_browser_callback, app);

    furi_string_reset(app->text);
    furi_string_cat(app->text, PATHPCAPS);
    file_browser_start(app->file_browser, app->text);

    view_dispatcher_switch_to_view(app->view_dispatcher, FileBrowserView);
}

// Function for the testing scene on event
bool app_scene_browser_pcaps_on_event(void* context, SceneManagerEvent event) {
    bool consumed = false;
    App* app = (App*)context;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

// Function for the testing scene on exit
void app_scene_browser_pcaps_on_exit(void* context) {
    App* app = (App*)context;
    file_browser_stop(app->file_browser);
}
