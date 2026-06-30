#include "../can_commander.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define CUSTOM_INJECT_SET_MAX      24U

static char cancommander_custom_inject_set_labels[CUSTOM_INJECT_SET_MAX][32];
static char cancommander_custom_inject_set_paths[CUSTOM_INJECT_SET_MAX][160];
static uint8_t cancommander_custom_inject_set_count = 0U;

static void cancommander_scene_custom_inject_load_slots_menu_callback(void* context, uint32_t index) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool cancommander_scene_custom_inject_has_suffix(const char* name, const char* suffix) {
    if(!name || !suffix) {
        return false;
    }

    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if(name_len <= suffix_len) {
        return false;
    }

    return strcmp(name + name_len - suffix_len, suffix) == 0;
}

static bool cancommander_scene_custom_inject_type_ok(const FuriString* file_type, uint32_t version) {
    if(!file_type || version != APP_SMART_INJECT_PROFILE_VER) {
        return false;
    }

    return furi_string_equal_str(file_type, APP_SMART_INJECT_PROFILE_FILETYPE) ||
           furi_string_equal_str(file_type, APP_SMART_INJECT_PROFILE_LEGACY_FILETYPE);
}

static void cancommander_scene_custom_inject_load_slots_start(App* app) {
    if(!app) {
        return;
    }

    // Always do a clean start so load->run behavior is deterministic.
    if(app->tool_active) {
        app_action_tool_stop(app);
    }
    app_action_tool_start(app, CcToolCustomInject, app->args_custom_inject_start, "custom_inject");

    if(app->connected && app->tool_active && app->dashboard_mode == AppDashboardCustomInject) {
        app_action_custom_inject_sync_slots(app);
    }
}

static void cancommander_scene_custom_inject_set_list_scan_dir(const char* dir_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(!storage_dir_open(dir, dir_path)) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo file_info;
    char entry_name[128] = {0};
    while(storage_dir_read(dir, &file_info, entry_name, sizeof(entry_name))) {
        const size_t len = strlen(entry_name);
        const bool is_new =
            cancommander_scene_custom_inject_has_suffix(entry_name, APP_SMART_INJECT_PROFILE_EXT);
        const bool is_legacy = cancommander_scene_custom_inject_has_suffix(
            entry_name, APP_SMART_INJECT_PROFILE_LEGACY_EXT);
        if(!is_new && !is_legacy) {
            continue;
        }
        if(cancommander_custom_inject_set_count >= CUSTOM_INJECT_SET_MAX) {
            break;
        }

        const uint8_t idx = cancommander_custom_inject_set_count;
        snprintf(
            cancommander_custom_inject_set_paths[idx],
            sizeof(cancommander_custom_inject_set_paths[idx]),
            "%s/%s",
            dir_path,
            entry_name);

        const size_t suffix_len =
            is_new ? strlen(APP_SMART_INJECT_PROFILE_EXT) :
                     strlen(APP_SMART_INJECT_PROFILE_LEGACY_EXT);
        size_t label_len = len - suffix_len;
        if(label_len >= sizeof(cancommander_custom_inject_set_labels[idx])) {
            label_len = sizeof(cancommander_custom_inject_set_labels[idx]) - 1U;
        }
        memcpy(cancommander_custom_inject_set_labels[idx], entry_name, label_len);
        cancommander_custom_inject_set_labels[idx][label_len] = '\0';

        FlipperFormat* ff = flipper_format_file_alloc(storage);
        FuriString* file_type = furi_string_alloc();
        FuriString* name_value = furi_string_alloc();
        if(ff && file_type && name_value) {
            if(flipper_format_file_open_existing(ff, cancommander_custom_inject_set_paths[idx])) {
                uint32_t version = 0U;
                if(
                    flipper_format_read_header(ff, file_type, &version) &&
                    cancommander_scene_custom_inject_type_ok(file_type, version) &&
                    flipper_format_read_string(ff, "name", name_value)) {
                    const char* display_name = furi_string_get_cstr(name_value);
                    if(display_name && display_name[0] != '\0') {
                        strncpy(
                            cancommander_custom_inject_set_labels[idx],
                            display_name,
                            sizeof(cancommander_custom_inject_set_labels[idx]) - 1U);
                        cancommander_custom_inject_set_labels[idx]
                                                            [sizeof(cancommander_custom_inject_set_labels[idx]) - 1U] =
                                                                '\0';
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

        cancommander_custom_inject_set_count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

static void cancommander_scene_custom_inject_set_list_scan(void) {
    cancommander_custom_inject_set_count = 0U;
    cancommander_scene_custom_inject_set_list_scan_dir(APP_SMART_INJECT_PROFILE_DIR);
    cancommander_scene_custom_inject_set_list_scan_dir(APP_SMART_INJECT_PROFILE_LEGACY_DIR);
}

void cancommander_scene_custom_inject_load_slots_menu_on_enter(void* context) {
    App* app = context;

    cancommander_scene_custom_inject_set_list_scan();

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Load Injection Profile");

    if(cancommander_custom_inject_set_count == 0U) {
        submenu_add_item(
            app->submenu,
            "No injection profiles",
            0xFFU,
            cancommander_scene_custom_inject_load_slots_menu_callback,
            app);
    } else {
        for(uint8_t i = 0U; i < cancommander_custom_inject_set_count; i++) {
            submenu_add_item(
                app->submenu,
                cancommander_custom_inject_set_labels[i],
                i,
                cancommander_scene_custom_inject_load_slots_menu_callback,
                app);
        }
    }

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(
            app->scene_manager, cancommander_scene_custom_inject_load_slots_menu));
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSubmenu);
}

bool cancommander_scene_custom_inject_load_slots_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    scene_manager_set_scene_state(
        app->scene_manager, cancommander_scene_custom_inject_load_slots_menu, event.event);

    if(
        cancommander_custom_inject_set_count == 0U || event.event >= cancommander_custom_inject_set_count) {
        return true;
    }

    if(app_custom_inject_load_slot_set_file(app, cancommander_custom_inject_set_paths[event.event])) {
        cancommander_scene_custom_inject_load_slots_start(app);
        const bool ready =
            app->connected && app->tool_active && app->dashboard_mode == AppDashboardCustomInject;
        scene_manager_next_scene(
            app->scene_manager,
            ready ? cancommander_scene_monitor : cancommander_scene_status);
        return true;
    }

    scene_manager_next_scene(app->scene_manager, cancommander_scene_status);
    return true;
}

void cancommander_scene_custom_inject_load_slots_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}
