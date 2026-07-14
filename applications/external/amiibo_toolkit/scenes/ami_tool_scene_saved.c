#include "ami_tool_i.h"
#include <string.h>
#include <ctype.h>
#include <nfc/nfc_device.h>

#define AMI_TOOL_SAVED_MENU_INDEX_PREV_PAGE UINT32_C(0xFFFFFFFC)
#define AMI_TOOL_SAVED_MENU_INDEX_NEXT_PAGE UINT32_C(0xFFFFFFFB)
#define AMI_TOOL_SAVED_FILENAME_MAX         254

typedef enum {
    AmiToolSavedLoadOk,
    AmiToolSavedLoadNoFolder,
    AmiToolSavedLoadError,
} AmiToolSavedLoadStatus;

static void ami_tool_scene_saved_show_message(AmiToolApp* app, const char* message);
static void ami_tool_scene_saved_show_menu(AmiToolApp* app);
static bool ami_tool_scene_saved_parse_filename(
    const char* name,
    char* id_hex,
    size_t id_size,
    char* uid_hex,
    size_t uid_size);
static AmiToolSavedLoadStatus ami_tool_scene_saved_load_page(AmiToolApp* app);
static void ami_tool_scene_saved_refresh(AmiToolApp* app);
static void ami_tool_scene_saved_submenu_callback(void* context, uint32_t index);
static bool ami_tool_scene_saved_load_entry(AmiToolApp* app, size_t index);

static void ami_tool_scene_saved_show_message(AmiToolApp* app, const char* message) {
    furi_assert(app);
    furi_string_set(
        app->text_box_store,
        message ? message : "No saved Amiibo files.\nUse Save to Storage first.");
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    app->saved_info_visible = false;
}

static bool ami_tool_scene_saved_is_hex_string(const char* str) {
    if(!str || *str == '\0') {
        return false;
    }
    while(*str) {
        if(!isxdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

static bool ami_tool_scene_saved_parse_filename(
    const char* name,
    char* id_hex,
    size_t id_size,
    char* uid_hex,
    size_t uid_size) {
    if(!name || !id_hex || !uid_hex || id_size == 0 || uid_size == 0) {
        return false;
    }

    size_t name_len = strlen(name);
    if(name_len <= 4) {
        return false;
    }

    const char* extension = name + name_len - 4;
    if(!(tolower((unsigned char)extension[0]) == '.' &&
         tolower((unsigned char)extension[1]) == 'n' &&
         tolower((unsigned char)extension[2]) == 'f' &&
         tolower((unsigned char)extension[3]) == 'c')) {
        return false;
    }

    size_t core_len = name_len - 4;
    char buffer[AMI_TOOL_SAVED_FILENAME_MAX + 1];
    if(core_len >= sizeof(buffer)) {
        return false;
    }
    memcpy(buffer, name, core_len);
    buffer[core_len] = '\0';

    char* dash = strrchr(buffer, '-');
    if(!dash) {
        return false;
    }

    *dash = '\0';
    const char* id_part = buffer;
    const char* uid_part = dash + 1;

    if(id_part[0] == '\0' || uid_part[0] == '\0') {
        return false;
    }
    if(!ami_tool_scene_saved_is_hex_string(id_part) ||
       !ami_tool_scene_saved_is_hex_string(uid_part)) {
        return false;
    }

    if(strlen(id_part) + 1 > id_size || strlen(uid_part) + 1 > uid_size) {
        return false;
    }

    for(size_t i = 0; id_part[i]; i++) {
        id_hex[i] = (char)toupper((unsigned char)id_part[i]);
    }
    id_hex[strlen(id_part)] = '\0';

    for(size_t i = 0; uid_part[i]; i++) {
        uid_hex[i] = (char)toupper((unsigned char)uid_part[i]);
    }
    uid_hex[strlen(uid_part)] = '\0';

    return true;
}

static AmiToolSavedLoadStatus ami_tool_scene_saved_load_page(AmiToolApp* app) {
    for(size_t i = 0; i < AMI_TOOL_SAVED_MAX_PAGE_ITEMS; i++) {
        furi_string_reset(app->saved_page_display[i]);
        furi_string_reset(app->saved_page_paths[i]);
        furi_string_reset(app->saved_page_ids[i]);
    }
    app->saved_page_entry_count = 0;
    app->saved_has_next_page = false;

    File* dir = storage_file_alloc(app->storage);
    if(!dir) {
        return AmiToolSavedLoadError;
    }

    if(storage_common_stat(app->storage, AMI_TOOL_NFC_FOLDER, NULL) != FSE_OK) {
        storage_file_free(dir);
        return AmiToolSavedLoadNoFolder;
    }

    if(!storage_dir_open(dir, AMI_TOOL_NFC_FOLDER)) {
        storage_file_free(dir);
        return AmiToolSavedLoadError;
    }

    FileInfo info;
    char name_buffer[AMI_TOOL_SAVED_FILENAME_MAX + 1];
    size_t valid_index = 0;
    size_t added = 0;
    bool has_next = false;
    FuriString* lookup_name = furi_string_alloc();

    while(storage_dir_read(dir, &info, name_buffer, sizeof(name_buffer))) {
        if(file_info_is_dir(&info)) {
            continue;
        }

        char id_hex[32] = {0};
        char uid_hex[32] = {0};
        if(!ami_tool_scene_saved_parse_filename(
               name_buffer, id_hex, sizeof(id_hex), uid_hex, sizeof(uid_hex))) {
            continue;
        }

        if(valid_index < app->saved_page_offset) {
            valid_index++;
            continue;
        }

        if(added < AMI_TOOL_SAVED_MAX_PAGE_ITEMS) {
            FuriString* path = app->saved_page_paths[added];
            FuriString* display = app->saved_page_display[added];
            FuriString* id_store = app->saved_page_ids[added];
            furi_string_printf(path, "%s/%s", AMI_TOOL_NFC_FOLDER, name_buffer);
            furi_string_set(id_store, id_hex);

            furi_string_reset(lookup_name);
            if(!ami_tool_info_get_name_for_id(app, id_hex, lookup_name)) {
                furi_string_set(lookup_name, id_hex);
            }

            furi_string_set(display, furi_string_get_cstr(lookup_name));
            furi_string_push_back(display, ' ');
            furi_string_cat(display, uid_hex);

            added++;
            valid_index++;
        } else {
            has_next = true;
            break;
        }
    }

    furi_string_free(lookup_name);
    storage_dir_close(dir);
    storage_file_free(dir);

    app->saved_page_entry_count = added;
    app->saved_has_next_page = has_next;

    return AmiToolSavedLoadOk;
}

static void ami_tool_scene_saved_show_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Saved Amiibo");

    if(app->saved_page_offset > 0) {
        submenu_add_item(
            app->submenu,
            "Prev Page",
            AMI_TOOL_SAVED_MENU_INDEX_PREV_PAGE,
            ami_tool_scene_saved_submenu_callback,
            app);
    }

    for(size_t i = 0; i < app->saved_page_entry_count; i++) {
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(app->saved_page_display[i]),
            i,
            ami_tool_scene_saved_submenu_callback,
            app);
    }

    if(app->saved_has_next_page) {
        submenu_add_item(
            app->submenu,
            "Next Page",
            AMI_TOOL_SAVED_MENU_INDEX_NEXT_PAGE,
            ami_tool_scene_saved_submenu_callback,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
    app->saved_info_visible = false;
}

static void ami_tool_scene_saved_refresh(AmiToolApp* app) {
    size_t attempts = 0;
    AmiToolSavedLoadStatus status;

    do {
        status = ami_tool_scene_saved_load_page(app);
        if(status != AmiToolSavedLoadOk) {
            break;
        }
        if(app->saved_page_entry_count == 0 && app->saved_page_offset > 0) {
            if(app->saved_page_offset >= AMI_TOOL_SAVED_MAX_PAGE_ITEMS) {
                app->saved_page_offset -= AMI_TOOL_SAVED_MAX_PAGE_ITEMS;
            } else {
                app->saved_page_offset = 0;
            }
        } else {
            break;
        }
    } while(++attempts < 2);

    if(status == AmiToolSavedLoadNoFolder) {
        ami_tool_scene_saved_show_message(
            app, "No saved Amiibo folder found.\nUse Save to Storage to create files.");
    } else if(status == AmiToolSavedLoadError) {
        ami_tool_scene_saved_show_message(
            app, "Unable to open saved Amiibo folder.\nCheck storage and try again.");
    } else if(app->saved_page_entry_count == 0) {
        ami_tool_scene_saved_show_message(
            app, "No saved Amiibo files found.\nUse Save to Storage first.");
    } else {
        ami_tool_scene_saved_show_menu(app);
    }
}

static bool ami_tool_scene_saved_load_entry(AmiToolApp* app, size_t index) {
    if(index >= app->saved_page_entry_count) {
        return false;
    }

    const char* path = furi_string_get_cstr(app->saved_page_paths[index]);
    if(!path || path[0] == '\0') {
        return false;
    }

    bool success = false;
    NfcDevice* device = nfc_device_alloc();
    if(!device) {
        return false;
    }

    do {
        if(!nfc_device_load(device, path)) {
            break;
        }
        const MfUltralightData* data =
            (const MfUltralightData*)nfc_device_get_data(device, NfcProtocolMfUltralight);
        if(!data || !app->tag_data) {
            break;
        }
        mf_ultralight_copy(app->tag_data, data);
        app->tag_data_valid = true;
        if(app->tag_data->pages_total > 0) {
            size_t pack_page = app->tag_data->pages_total - 1;
            memcpy(app->tag_pack, app->tag_data->page[pack_page].data, sizeof(app->tag_pack));
            app->tag_pack_valid = true;
        }

        size_t uid_len = 0;
        const uint8_t* uid = mf_ultralight_get_uid(app->tag_data, &uid_len);
        if(uid && uid_len > 0) {
            ami_tool_store_uid(app, uid, uid_len);
            if(ami_tool_compute_password_from_uid(uid, uid_len, &app->tag_password)) {
                app->tag_password_valid = true;
            } else {
                app->tag_password_valid = false;
                memset(&app->tag_password, 0, sizeof(app->tag_password));
            }
        } else {
            app->tag_password_valid = false;
            memset(&app->tag_password, 0, sizeof(app->tag_password));
        }

        amiibo_configure_rf_interface(app->tag_data);
        const char* id_hex = furi_string_get_cstr(app->saved_page_ids[index]);
        ami_tool_info_show_page(app, (id_hex && id_hex[0]) ? id_hex : NULL, false);
        app->saved_info_visible = true;
        success = true;
    } while(false);

    nfc_device_free(device);
    return success;
}

static void ami_tool_scene_saved_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;

    if(index == AMI_TOOL_SAVED_MENU_INDEX_PREV_PAGE) {
        if(app->saved_page_offset >= AMI_TOOL_SAVED_MAX_PAGE_ITEMS) {
            app->saved_page_offset -= AMI_TOOL_SAVED_MAX_PAGE_ITEMS;
        } else {
            app->saved_page_offset = 0;
        }
        ami_tool_scene_saved_refresh(app);
    } else if(index == AMI_TOOL_SAVED_MENU_INDEX_NEXT_PAGE) {
        app->saved_page_offset += app->saved_page_entry_count;
        ami_tool_scene_saved_refresh(app);
    } else {
        if(!ami_tool_scene_saved_load_entry(app, index)) {
            ami_tool_scene_saved_show_message(
                app, "Unable to load Amiibo file.\nCheck the NFC file and try again.");
        }
    }
}

void ami_tool_scene_saved_on_enter(void* context) {
    AmiToolApp* app = context;
    app->saved_page_offset = 0;
    app->saved_info_visible = false;
    ami_tool_scene_saved_refresh(app);
}

bool ami_tool_scene_saved_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventInfoWriteStarted:
        case AmiToolEventInfoWriteSuccess:
        case AmiToolEventInfoWriteFailed:
        case AmiToolEventInfoWriteCancelled:
            ami_tool_info_handle_write_event(app, event.event);
            return true;
        case AmiToolEventInfoShowActions:
            ami_tool_info_show_actions_menu(app);
            return true;
        case AmiToolEventInfoActionEmulate:
            if(!ami_tool_info_start_emulation(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to start emulation.\nLoad a saved Amiibo first.");
            }
            return true;
        case AmiToolEventInfoActionUsage:
            ami_tool_info_show_usage(app);
            return true;
        case AmiToolEventInfoActionChangeUid:
            if(!ami_tool_info_change_uid(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to change UID.\nInstall key_retail.bin and try again.");
            }
            return true;
        case AmiToolEventInfoActionWriteTag:
            if(!ami_tool_info_write_to_tag(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to write tag.\nUse a blank NTAG215 and make\nsure key_retail.bin is installed.");
            }
            return true;
        case AmiToolEventInfoActionSaveToStorage:
            if(!ami_tool_info_save_to_storage(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to save Amiibo.\nLoad one first and check storage access.");
            }
            return true;
        case AmiToolEventUsagePrevPage:
            ami_tool_info_navigate_usage(app, -1);
            return true;
        case AmiToolEventUsageNextPage:
            ami_tool_info_navigate_usage(app, 1);
            return true;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->write_in_progress) {
            if(app->write_waiting_for_tag) {
                ami_tool_info_request_write_cancel(app);
            }
            return true;
        }
        if(app->info_emulation_active) {
            ami_tool_info_stop_emulation(app);
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->usage_info_visible) {
            app->usage_info_visible = false;
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_action_message_visible) {
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_actions_visible) {
            app->info_actions_visible = false;
            ami_tool_info_refresh_current_page(app);
            return true;
        }
        if(app->saved_info_visible) {
            ami_tool_scene_saved_show_menu(app);
            return true;
        }
        return false;
    }

    return false;
}

void ami_tool_scene_saved_on_exit(void* context) {
    AmiToolApp* app = context;
    ami_tool_info_stop_emulation(app);
    ami_tool_info_abort_write(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    app->saved_info_visible = false;
}
