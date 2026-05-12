#include "scenes.h"

#define BITRAW_DEFAULT_PATH EXT_PATH("subghz")

void bitraw_scene_browser_on_enter(void* context) {
    BitrawApp* app = context;

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, ".sub", NULL);
    options.base_path = BITRAW_DEFAULT_PATH;

    FuriString* preselect = furi_string_alloc_set(BITRAW_DEFAULT_PATH);
    bool picked =
        dialog_file_browser_show(app->dialogs, app->selected_path, preselect, &options);
    furi_string_free(preselect);

    if(picked) {
        scene_manager_next_scene(app->scene_manager, BitrawSceneAnalyzing);
    } else {
        view_dispatcher_stop(app->view_dispatcher);
    }
}

bool bitraw_scene_browser_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bitraw_scene_browser_on_exit(void* context) {
    UNUSED(context);
}
