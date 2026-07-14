#include "../nfc_dict_manager.h"
#include "../nfc_dict_manager_utils.h"

void nfc_dict_manager_scene_merge_select_b_on_enter(void* context) {
    NfcDictManager* app = context;

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, DICT_EXTENSION, &I_file_10px);
    browser_options.base_path = DICT_FOLDER_PATH;
    browser_options.skip_assets = false;

    FuriString* path = furi_string_alloc();
    furi_string_set(path, DICT_FOLDER_PATH);

    bool success =
        dialog_file_browser_show(app->dialogs, app->selected_dict_b, path, &browser_options);
    furi_string_free(path);

    if(success) {
        scene_manager_previous_scene(app->scene_manager);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool nfc_dict_manager_scene_merge_select_b_on_event(void* context, SceneManagerEvent event) {
    NfcDictManager* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void nfc_dict_manager_scene_merge_select_b_on_exit(void* context) {
    UNUSED(context);
}
