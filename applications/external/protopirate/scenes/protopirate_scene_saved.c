// scenes/protopirate_scene_saved.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"

#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSceneSaved"

void protopirate_scene_saved_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        if(!storage_dir_exists(storage, PROTOPIRATE_APP_FOLDER)) {
            storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
        }
        furi_record_close(RECORD_STORAGE);
    }

    if(!app->file_path) {
        FURI_LOG_E(TAG, "file_path is NULL");
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!app->dialogs) {
        app->dialogs = furi_record_open(RECORD_DIALOGS);
        if(!app->dialogs) {
            FURI_LOG_E(TAG, "Failed to open dialogs");
            scene_manager_previous_scene(app->scene_manager);
            return;
        }
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".psf", &I_subghz_10px);
    browser_options.base_path = PROTOPIRATE_APP_FOLDER;
    browser_options.skip_assets = true;
    browser_options.hide_dot_files = true;

    furi_string_set(app->file_path, PROTOPIRATE_APP_FOLDER);

    FuriString* selection = furi_string_alloc();
    if(app->loaded_file_path && !furi_string_empty(app->loaded_file_path)) {
        furi_string_set(selection, app->loaded_file_path);
    } else {
        furi_string_set(selection, PROTOPIRATE_APP_FOLDER);
    }

    bool file_selected =
        dialog_file_browser_show(app->dialogs, selection, app->file_path, &browser_options);

    if(file_selected) {
        if(app->loaded_file_path) {
            furi_string_free(app->loaded_file_path);
        }
        app->loaded_file_path = furi_string_alloc_set(selection);
        furi_string_free(selection);
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSavedInfo);
    } else {
        furi_string_free(selection);
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool protopirate_scene_saved_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void protopirate_scene_saved_on_exit(void* context) {
    UNUSED(context);
}
