#include "../switch_controller_app.h"
#include <storage/storage.h>

typedef struct {
    char name[MACRO_NAME_MAX_LEN];
    char path[256];
} MacroListItem;

static MacroListItem* macro_list_items = NULL;
static size_t macro_list_count = 0;

static void switch_controller_scene_macro_list_item_callback(void* context, uint32_t index) {
    SwitchControllerApp* app = context;

    // Load the selected macro
    if(index < macro_list_count) {
        Macro* loaded_macro = macro_load(macro_list_items[index].path);
        if(loaded_macro) {
            // Free current macro and use loaded one
            macro_free(app->current_macro);
            app->current_macro = loaded_macro;

            // Go to playing scene
            view_dispatcher_send_custom_event(app->view_dispatcher, index);
        }
    }
}

void switch_controller_scene_on_enter_macro_list(void* context) {
    SwitchControllerApp* app = context;

    // Clear existing list
    variable_item_list_reset(app->macro_list);

    // Free previous list items
    if(macro_list_items) {
        free(macro_list_items);
        macro_list_items = NULL;
    }
    macro_list_count = 0;

    // Scan for macro files
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    if(storage_dir_open(dir, SWITCH_CONTROLLER_APP_PATH_FOLDER)) {
        FileInfo file_info;
        char filename[128];

        // Count files first
        size_t file_count = 0;
        while(storage_dir_read(dir, &file_info, filename, sizeof(filename))) {
            if(!file_info_is_dir(&file_info)) {
                // Check extension
                const char* ext = strrchr(filename, '.');
                if(ext && strcmp(ext, SWITCH_CONTROLLER_MACRO_EXTENSION) == 0) {
                    file_count++;
                }
            }
        }

        if(file_count > 0) {
            // Allocate list
            macro_list_items = malloc(sizeof(MacroListItem) * file_count);
            macro_list_count = 0;

            // Rewind and read again
            storage_dir_close(dir);
            storage_dir_open(dir, SWITCH_CONTROLLER_APP_PATH_FOLDER);

            while(storage_dir_read(dir, &file_info, filename, sizeof(filename))) {
                if(!file_info_is_dir(&file_info)) {
                    const char* ext = strrchr(filename, '.');
                    if(ext && strcmp(ext, SWITCH_CONTROLLER_MACRO_EXTENSION) == 0) {
                        // Add to list
                        MacroListItem* item = &macro_list_items[macro_list_count];

                        // Extract name (without extension)
                        size_t name_len = ext - filename;
                        if(name_len >= MACRO_NAME_MAX_LEN) {
                            name_len = MACRO_NAME_MAX_LEN - 1;
                        }
                        memcpy(item->name, filename, name_len);
                        item->name[name_len] = '\0';

                        // Full path
                        snprintf(
                            item->path,
                            sizeof(item->path),
                            "%s/%s",
                            SWITCH_CONTROLLER_APP_PATH_FOLDER,
                            filename);

                        // Add to variable item list
                        variable_item_list_add(app->macro_list, item->name, 0, NULL, NULL);

                        macro_list_count++;
                    }
                }
            }
        }

        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    if(macro_list_count == 0) {
        variable_item_list_add(app->macro_list, "No macros found", 0, NULL, NULL);
    } else {
        variable_item_list_set_enter_callback(
            app->macro_list, switch_controller_scene_macro_list_item_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewMacroList);
}

bool switch_controller_scene_on_event_macro_list(void* context, SceneManagerEvent event) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Macro selected, go to playing scene
        scene_manager_next_scene(app->scene_manager, SwitchControllerScenePlaying);
        consumed = true;
    }

    return consumed;
}

void switch_controller_scene_on_exit_macro_list(void* context) {
    SwitchControllerApp* app = context;
    variable_item_list_reset(app->macro_list);
}
