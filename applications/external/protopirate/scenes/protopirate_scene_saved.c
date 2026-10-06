// scenes/protopirate_scene_saved.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"

#include "proto_pirate_icons.h"

#define TAG "PPSceneSaved"

void protopirate_scene_saved_on_enter(void* context) {
    ProtoPirateApp* app = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        if(!storage_dir_exists(storage, PROTOPIRATE_APP_FOLDER)) {
            storage_simply_mkdir(storage, PROTOPIRATE_APP_FOLDER);
        }
        furi_record_close(RECORD_STORAGE);
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
    dialog_file_browser_set_basic_options(&browser_options, ".psf", &I_protopirate_10px);
    browser_options.base_path = PROTOPIRATE_APP_FOLDER;
    browser_options.skip_assets = true;
    browser_options.hide_dot_files = true;

    FuriString* selection = furi_string_alloc();
    if(app->loaded_file_path && (strlen(app->loaded_file_path) > 0)) {
        furi_string_set(selection, app->loaded_file_path);
    } else {
        furi_string_set(selection, PROTOPIRATE_APP_FOLDER);
    }

    FuriString* file_path_furi_str = furi_string_alloc_set_str(app->file_path);
    bool file_selected =
        dialog_file_browser_show(app->dialogs, selection, file_path_furi_str, &browser_options);
    furi_string_free(file_path_furi_str);

    if(file_selected) {
        FURI_LOG_D("TEST", "Loading new file");
        if(app->loaded_file_path) {
            free(app->loaded_file_path);
            app->loaded_file_path = NULL;
        }
        size_t len = furi_string_utf8_length(selection) + 1;
        app->loaded_file_path = malloc(len);
        snprintf(app->loaded_file_path, len, furi_string_get_cstr(selection));
        furi_string_free(selection);
        scene_manager_next_scene(app->scene_manager, ProtoPirateSceneSavedInfo);
    } else {
        FURI_LOG_D("TEST", "Leaving");

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
