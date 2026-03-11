#include "../can_commander.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define APP_DBC_CONFIG_DIR  APP_DATA_PATH("dbc_configs")
#define APP_DBC_CONFIG_TYPE "CANCommanderDbcConfig"
#define APP_DBC_CONFIG_VER  1U
#define DBC_CONFIG_MAX      24U

static char cancommander_dbc_config_labels[DBC_CONFIG_MAX][32];
static char cancommander_dbc_config_paths[DBC_CONFIG_MAX][160];
static uint8_t cancommander_dbc_config_count = 0U;

static void cancommander_scene_dbc_load_config_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cancommander_scene_dbc_config_list_scan(void) {
    cancommander_dbc_config_count = 0U;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, APP_DBC_CONFIG_DIR)) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo file_info;
    char entry_name[128] = {0};
    while(storage_dir_read(dir, &file_info, entry_name, sizeof(entry_name))) {
        const size_t len = strlen(entry_name);
        if(len <= 5U || strcmp(entry_name + len - 5U, ".dcfg") != 0) {
            continue;
        }
        if(cancommander_dbc_config_count >= DBC_CONFIG_MAX) {
            break;
        }

        const uint8_t idx = cancommander_dbc_config_count;
        snprintf(
            cancommander_dbc_config_paths[idx],
            sizeof(cancommander_dbc_config_paths[idx]),
            "%s/%s",
            APP_DBC_CONFIG_DIR,
            entry_name);

        size_t label_len = len - 5U;
        if(label_len >= sizeof(cancommander_dbc_config_labels[idx])) {
            label_len = sizeof(cancommander_dbc_config_labels[idx]) - 1U;
        }
        memcpy(cancommander_dbc_config_labels[idx], entry_name, label_len);
        cancommander_dbc_config_labels[idx][label_len] = '\0';

        FlipperFormat* ff = flipper_format_file_alloc(storage);
        FuriString* file_type = furi_string_alloc();
        FuriString* name_value = furi_string_alloc();
        if(ff && file_type && name_value) {
            if(flipper_format_file_open_existing(ff, cancommander_dbc_config_paths[idx])) {
                uint32_t version = 0U;
                if(flipper_format_read_header(ff, file_type, &version) &&
                   furi_string_equal_str(file_type, APP_DBC_CONFIG_TYPE) &&
                   version == APP_DBC_CONFIG_VER &&
                   flipper_format_read_string(ff, "name", name_value)) {
                    const char* display_name = furi_string_get_cstr(name_value);
                    if(display_name && display_name[0] != '\0') {
                        strncpy(
                            cancommander_dbc_config_labels[idx],
                            display_name,
                            sizeof(cancommander_dbc_config_labels[idx]) - 1U);
                        cancommander_dbc_config_labels
                            [idx][sizeof(cancommander_dbc_config_labels[idx]) - 1U] = '\0';
                    }
                }
            }
        }
        if(name_value) {
            furi_string_free(name_value);
        }
        if(file_type) {
            furi_string_free(file_type);
        }
        if(ff) {
            flipper_format_free(ff);
        }

        cancommander_dbc_config_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

void cancommander_scene_dbc_load_config_menu_on_enter(void* context) {
    App* app = context;

    cancommander_scene_dbc_config_list_scan();

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Load DBC Config");

    if(cancommander_dbc_config_count == 0U) {
        submenu_add_item(
            app->submenu,
            "No saved DBC configs",
            0xFFU,
            cancommander_scene_dbc_load_config_menu_callback,
            app);
    } else {
        for(uint8_t i = 0U; i < cancommander_dbc_config_count; i++) {
            submenu_add_item(
                app->submenu,
                cancommander_dbc_config_labels[i],
                i,
                cancommander_scene_dbc_load_config_menu_callback,
                app);
        }
    }

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, cancommander_scene_dbc_load_config_menu));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_dbc_load_config_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_dbc_load_config_menu, event.event);

    if(cancommander_dbc_config_count == 0U || event.event >= cancommander_dbc_config_count) {
        return true;
    }

    (void)app_dbc_config_load_file(app, cancommander_dbc_config_paths[event.event], true);
    scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
    return true;
}

void cancommander_scene_dbc_load_config_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
