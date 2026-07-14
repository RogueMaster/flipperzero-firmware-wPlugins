#include "../clippy_app_i.h"
#include <storage/storage.h>

static bool clippy_layout_select(ClippyApp* clippy) {
    furi_assert(clippy);

    FuriString* predefined_path;
    predefined_path = furi_string_alloc();
    if(!furi_string_empty(clippy->keyboard_layout)) {
        furi_string_set(predefined_path, clippy->keyboard_layout);
    } else {
        furi_string_set(predefined_path, CLIPPY_APP_PATH_LAYOUT_FOLDER);
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(
        &browser_options, CLIPPY_APP_LAYOUT_EXTENSION, &I_keyboard_10px);
    browser_options.base_path = CLIPPY_APP_PATH_LAYOUT_FOLDER;
    browser_options.skip_assets = false;

    // Input events and views are managed by file_browser
    bool res = dialog_file_browser_show(
        clippy->dialogs, clippy->keyboard_layout, predefined_path, &browser_options);

    furi_string_free(predefined_path);
    return res;
}

void clippy_scene_config_layout_on_enter(void* context) {
    ClippyApp* clippy = context;

    if(clippy_layout_select(clippy)) {
        scene_manager_search_and_switch_to_previous_scene(
            clippy->scene_manager, ClippySceneBadUsbWork);
    } else {
        scene_manager_previous_scene(clippy->scene_manager);
    }
}

bool clippy_scene_config_layout_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // ClippyApp* clippy = context;
    return false;
}

void clippy_scene_config_layout_on_exit(void* context) {
    UNUSED(context);
    // ClippyApp* clippy = context;
}
