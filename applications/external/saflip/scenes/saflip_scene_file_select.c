#include "../saflip.h"
#include "../util/storage.h"

void saflip_scene_file_select_on_enter(void* context) {
    SaflipApp* app = context;

    FuriString* saflip_app_folder;
    saflip_app_folder = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, SAFLIP_APP_EXTENSION, &I_125_10px);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

    bool success = false;
    FuriString* path = furi_string_alloc();
    if(dialog_file_browser_show(app->dialogs_app, path, saflip_app_folder, &browser_options)) {
        if(saflip_load_file(app, furi_string_get_cstr(path))) {
            success = true;
            scene_manager_next_scene(app->scene_manager, SaflipSceneInfo);
        }
    }
    furi_string_free(saflip_app_folder);

    if(!success) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SaflipSceneStart);
    }
}

bool saflip_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void saflip_scene_file_select_on_exit(void* context) {
    UNUSED(context);
}
