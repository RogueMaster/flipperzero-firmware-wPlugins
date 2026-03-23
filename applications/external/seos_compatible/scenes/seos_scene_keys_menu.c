#include "../seos_i.h"

#define TAG           "SceneKeysMenu"
#define MAX_KEY_FILES 16

enum SubmenuIndex {
    SubmenuIndexZeroKeys = 0,
    SubmenuIndexKeyFileStart = 1,
};

static char key_filenames[MAX_KEY_FILES][SEOS_FILE_NAME_MAX_LENGTH + 1];
static uint8_t key_file_count = 0;

// Returns base name length (without .txt) if file matches "keys.txt" or "*_keys.txt" pattern, 0 otherwise
static size_t keyset_name_length(const char* name, size_t len) {
    if(len < 5) return 0;
    if(strcmp(name + len - 4, ".txt") != 0) return 0;
    bool is_keys = (strcmp(name, "keys.txt") == 0);
    bool is_x_keys = (len > 9 && strcmp(name + len - 9, "_keys.txt") == 0);
    if(!is_keys && !is_x_keys) return 0;
    size_t base_len = len - 4;
    if(base_len > SEOS_FILE_NAME_MAX_LENGTH) return 0;
    return base_len;
}

void seos_scene_keys_menu_submenu_callback(void* context, uint32_t index) {
    Seos* seos = context;
    view_dispatcher_send_custom_event(seos->view_dispatcher, index);
}

void seos_scene_keys_menu_on_enter(void* context) {
    Seos* seos = context;
    Submenu* submenu = seos->submenu;
    submenu_reset(submenu);
    key_file_count = 0;

    bool zero_active = (seos->keys_version == 0);
    submenu_add_item(
        submenu,
        zero_active ? "Zero Keys *" : "Zero Keys",
        SubmenuIndexZeroKeys,
        seos_scene_keys_menu_submenu_callback,
        seos);

    File* dir = storage_file_alloc(seos->credential->storage);
    if(storage_dir_open(dir, STORAGE_APP_DATA_PATH_PREFIX)) {
        FileInfo info;
        char name[256];
        while(storage_dir_read(dir, &info, name, sizeof(name)) && key_file_count < MAX_KEY_FILES) {
            if(info.flags & FSF_DIRECTORY) continue;
            size_t base_len = keyset_name_length(name, strlen(name));
            if(!base_len) continue;

            name[base_len] = '\0';
            strncpy(key_filenames[key_file_count], name, SEOS_FILE_NAME_MAX_LENGTH);
            key_filenames[key_file_count][SEOS_FILE_NAME_MAX_LENGTH] = '\0';

            bool is_active = seos->keys_version > 0 &&
                             furi_string_equal_str(seos->active_key_file, name);

            char label[SEOS_FILE_NAME_MAX_LENGTH + 4];
            snprintf(label, sizeof(label), is_active ? "%s *" : "%s", name);

            submenu_add_item(
                submenu,
                label,
                SubmenuIndexKeyFileStart + key_file_count,
                seos_scene_keys_menu_submenu_callback,
                seos);

            key_file_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    submenu_set_selected_item(
        seos->submenu, scene_manager_get_scene_state(seos->scene_manager, SeosSceneKeysMenu));

    view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewMenu);
}

bool seos_scene_keys_menu_on_event(void* context, SceneManagerEvent event) {
    Seos* seos = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SubmenuIndexZeroKeys) {
            seos_reset_to_zero_keys(seos);
            scene_manager_search_and_switch_to_previous_scene(
                seos->scene_manager, SeosSceneMainMenu);
            consumed = true;
        } else if(
            event.event >= SubmenuIndexKeyFileStart &&
            event.event < (uint32_t)(SubmenuIndexKeyFileStart + key_file_count)) {
            uint8_t file_index = event.event - SubmenuIndexKeyFileStart;

            if(seos_load_keys_from_file(seos, key_filenames[file_index])) {
                if(seos->keys_version == 1) {
                    scene_manager_next_scene(seos->scene_manager, SeosSceneMigrateKeys);
                } else {
                    scene_manager_search_and_switch_to_previous_scene(
                        seos->scene_manager, SeosSceneMainMenu);
                }
            } else {
                popup_set_header(seos->popup, "Load Failed", 64, 20, AlignCenter, AlignTop);
                popup_set_text(
                    seos->popup, "Could not load key file", 64, 40, AlignCenter, AlignTop);
                popup_set_timeout(seos->popup, 2000);
                popup_enable_timeout(seos->popup);
                view_dispatcher_switch_to_view(seos->view_dispatcher, SeosViewPopup);
            }
            consumed = true;
        }
    }

    return consumed;
}

void seos_scene_keys_menu_on_exit(void* context) {
    Seos* seos = context;
    submenu_reset(seos->submenu);
}
