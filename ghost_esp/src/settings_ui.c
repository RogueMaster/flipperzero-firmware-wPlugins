#include "settings_ui.h"
#include "settings_ui_types.h"
#include "settings_def.h"
#include "app_state.h"
#include "menu.h"
#include "sequential_file.h"
#include "uart_utils.h"
#include <furi.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>
#include "settings_storage.h"
#include "utils.h"
#include "callbacks.h"

#define MAX_FILENAME_LEN 256
#define MAX_PATH_LEN     512

static inline void close_current_log(AppState* app) {
    if(app && app->uart_context && app->uart_context->storageContext &&
       app->uart_context->storageContext->log_file) {
        storage_file_close(app->uart_context->storageContext->log_file);
    }
}

static inline void create_new_log(AppState* app) {
    if(app && app->uart_context && app->uart_context->storageContext) {
        sequential_file_open(
            app->uart_context->storageContext->storage_api,
            app->uart_context->storageContext->log_file,
            GHOST_ESP_APP_FOLDER_LOGS,
            "ghost_logs",
            "txt");
    }
}

void clear_log_files(void* context) {
    AppState* app = (AppState*)context;
    if(!app) return;

    // Close current log file
    close_current_log(app);

    // Stack allocation for better performance
    char filename[MAX_FILENAME_LEN];
    char full_path[MAX_PATH_LEN];
    FileInfo file_info;
    int deleted_count = 0;

    // Open storage once
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(!dir) {
        furi_record_close(RECORD_STORAGE);
        create_new_log(app);
        return;
    }

    if(!storage_dir_open(dir, GHOST_ESP_APP_FOLDER_LOGS)) {
        FURI_LOG_E("ClearLogs", "Failed to open logs directory");
        goto cleanup;
    }

    // Batch process files
    while(storage_dir_read(dir, &file_info, filename, MAX_FILENAME_LEN)) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        // Construct path only for files
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", GHOST_ESP_APP_FOLDER_LOGS, filename);

        // Remove file directly without extra existence check
        if(storage_simply_remove(storage, full_path)) {
            deleted_count++;
        }
    }

    FURI_LOG_I("ClearLogs", "Deleted %d files", deleted_count);

cleanup:
    // Cleanup resources
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    // Create new log file
    create_new_log(app);
}

void clear_pcap_files(void* context) {
    AppState* app = (AppState*)context;
    if(!app) return;

    // Close current file if open
    if(app->uart_context && app->uart_context->storageContext &&
       app->uart_context->storageContext->current_file) {
        if(storage_file_is_open(app->uart_context->storageContext->current_file)) {
            storage_file_sync(app->uart_context->storageContext->current_file);
            storage_file_close(app->uart_context->storageContext->current_file);
        }
        app->uart_context->storageContext->HasOpenedFile = false;
    }

    // Stack allocation for better performance
    char filename[MAX_FILENAME_LEN];
    char full_path[MAX_PATH_LEN];
    FileInfo file_info;
    int deleted_count = 0;

    // Open storage once
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(!dir) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(!storage_dir_open(dir, GHOST_ESP_APP_FOLDER_PCAPS)) {
        FURI_LOG_E("ClearPCAPs", "Failed to open pcaps directory");
        goto cleanup;
    }

    // Batch process files
    while(storage_dir_read(dir, &file_info, filename, MAX_FILENAME_LEN)) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        snprintf(full_path, MAX_PATH_LEN, "%s/%s", GHOST_ESP_APP_FOLDER_PCAPS, filename);

        if(storage_simply_remove(storage, full_path)) {
            deleted_count++;
        }
    }

    FURI_LOG_I("ClearPCAPs", "Deleted %d files", deleted_count);

cleanup:
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

void clear_wardrive_files(void* context) {
    AppState* app = (AppState*)context;
    if(!app) return;

    // Stack allocation for better performance
    char filename[MAX_FILENAME_LEN];
    char full_path[MAX_PATH_LEN];
    FileInfo file_info;
    int deleted_count = 0;

    // Open storage once
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    if(!dir) {
        furi_record_close(RECORD_STORAGE);
        return;
    }

    if(!storage_dir_open(dir, GHOST_ESP_APP_FOLDER_WARDRIVE)) {
        FURI_LOG_E("ClearWardrive", "Failed to open wardrive directory");
        goto cleanup;
    }

    // Batch process files
    while(storage_dir_read(dir, &file_info, filename, MAX_FILENAME_LEN)) {
        if(file_info.flags & FSF_DIRECTORY) continue;

        snprintf(full_path, MAX_PATH_LEN, "%s/%s", GHOST_ESP_APP_FOLDER_WARDRIVE, filename);

        if(storage_simply_remove(storage, full_path)) {
            deleted_count++;
        }
    }

    FURI_LOG_I("ClearWardrive", "Deleted %d files", deleted_count);

cleanup:
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

bool settings_set(Settings* settings, SettingKey key, uint8_t value, void* context) {
    FURI_LOG_D("SettingsSet", "Entering settings_set function for key: %d, value: %d", key, value);

    if(key >= SETTINGS_COUNT) return false;
    bool changed = false;

    switch(key) {
    case SETTING_RGB_MODE:
        if(settings->rgb_mode_index != value) {
            settings->rgb_mode_index = value;
            changed = true;
        }
        break;

    case SETTING_CHANNEL_HOP_DELAY:
        if(settings->channel_hop_delay_index != value) {
            settings->channel_hop_delay_index = value;
            changed = true;
        }
        break;

    case SETTING_ENABLE_CHANNEL_HOPPING:
        if(settings->enable_channel_hopping_index != value) {
            settings->enable_channel_hopping_index = value;
            changed = true;
        }
        break;

    case SETTING_ENABLE_RANDOM_BLE_MAC:
        if(settings->enable_random_ble_mac_index != value) {
            settings->enable_random_ble_mac_index = value;
            changed = true;
        }
        break;

    case SETTING_STOP_ON_BACK:
        if(settings->stop_on_back_index != value) {
            settings->stop_on_back_index = value;
            changed = true;
        }
        break;

    case SETTING_ENABLE_FILTERING:
        if(settings->enable_filtering_index != value) {
            settings->enable_filtering_index = value;
            if(context) {
                SettingsUIContext* settings_context = (SettingsUIContext*)context;
                if(settings_context->context) {
                    AppState* app_state = (AppState*)settings_context->context;
                    app_state->filter_config.enabled = value;
                }
            }
            changed = true;
        }
        break;
    case SETTING_SHOW_INFO:
        if(value == 0) { // Execute on press
            SettingsUIContext* settings_context = (SettingsUIContext*)context;
            if(settings_context && settings_context->context) {
                FURI_LOG_I("SettingsSet", "Posting custom event to show app info dialog");
                AppState* app_state = (AppState*)settings_context->context;
                // Post a custom event with the key
                view_dispatcher_send_custom_event(app_state->view_dispatcher, key);
            }
        }
        break;

    case SETTING_REBOOT_ESP:
        if(value == 0) { // Execute on press
            SettingsUIContext* settings_context = (SettingsUIContext*)context;
            if(settings_context && settings_context->send_uart_command) {
                FURI_LOG_I("SettingsSet", "Executing reboot command");
                settings_context->send_uart_command("handle_reboot\n", settings_context->context);
            }
        }
        break;

    case SETTING_CLEAR_LOGS:
    case SETTING_CLEAR_NVS:
    case SETTING_CLEAR_PCAPS:
    case SETTING_CLEAR_WARDRIVE:
        if(value == 0) { // Execute on press
            SettingsUIContext* settings_context = (SettingsUIContext*)context;
            if(settings_context && settings_context->context) {
                FURI_LOG_I("SettingsSet", "Posting custom event to show confirmation dialog");
                AppState* app_state = (AppState*)settings_context->context;
                // Post a custom event with the key
                view_dispatcher_send_custom_event(app_state->view_dispatcher, key);
            }
        }
        break;
    case SETTING_VIEW_LOGS_FROM_START:
        if(settings->view_logs_from_start_index != value) {
            settings->view_logs_from_start_index = value;
            changed = true;
        }
        break;

    case SETTING_DISABLE_ESP_CHECK:
        if(settings->disable_esp_check_index != value) {
            settings->disable_esp_check_index = value;
            changed = true;
        }
        break;

    default:
        return false;
    }

    if(changed) {
        FURI_LOG_I("SettingsSet", "Setting changed, saving to storage");
        // Save settings to storage
        settings_storage_save(settings, GHOST_ESP_APP_SETTINGS_FILE);
    }

    return true;
}

uint8_t settings_get(const Settings* settings, SettingKey key) {
    FURI_LOG_D("SettingsGet", "Getting setting for key: %d", key);

    switch(key) {
    case SETTING_RGB_MODE:
        return settings->rgb_mode_index;

    case SETTING_CHANNEL_HOP_DELAY:
        return settings->channel_hop_delay_index;

    case SETTING_ENABLE_CHANNEL_HOPPING:
        return settings->enable_channel_hopping_index;

    case SETTING_ENABLE_RANDOM_BLE_MAC:
        return settings->enable_random_ble_mac_index;

    case SETTING_STOP_ON_BACK:
        return settings->stop_on_back_index;

    case SETTING_ENABLE_FILTERING:
        return settings->enable_filtering_index;

    case SETTING_VIEW_LOGS_FROM_START:
        return settings->view_logs_from_start_index;

    case SETTING_DISABLE_ESP_CHECK:
        return settings->disable_esp_check_index;

    case SETTING_REBOOT_ESP:
    case SETTING_CLEAR_LOGS:
    case SETTING_CLEAR_NVS:
        return 0;

    default:
        return 0;
    }
}

static void settings_item_change_callback(VariableItem* item) {
    FURI_LOG_D("SettingsChange", "Settings item change callback triggered");

    VariableItemContext* item_context = variable_item_get_context(item);
    if(!item_context) {
        FURI_LOG_E("SettingsChange", "Invalid item context");
        return;
    }

    SettingsUIContext* context = item_context->settings_ui_context;
    SettingKey key = item_context->key;

    FURI_LOG_D("SettingsChange", "Handling key: %d", key);

    const SettingMetadata* metadata = settings_get_metadata(key);
    if(!metadata) {
        FURI_LOG_E("SettingsChange", "Invalid metadata for key: %d", key);
        return;
    }

    if(metadata->is_action) {
        // Action button pressed
        FURI_LOG_D("SettingsChange", "Action button detected for key: %d", key);

        // Reset the action button's value to allow future presses
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, metadata->data.action.name);
        FURI_LOG_D("SettingsChange", "Action button state reset for key: %d", key);

        // Execute the action
        if(metadata->data.action.callback) {
            FURI_LOG_D("SettingsChange", "Executing callback for action key: %d", key);
            metadata->data.action.callback(context);
            FURI_LOG_I("SettingsChange", "Action callback executed for key: %d", key);
        } else if(metadata->data.action.command && context->send_uart_command) {
            FURI_LOG_D("SettingsChange", "Executing command for action key: %d", key);
            context->send_uart_command(metadata->data.action.command, context->context);
            FURI_LOG_I("SettingsChange", "Action command executed for key: %d", key);
        } else {
            FURI_LOG_I("SettingsChange", "No action handler found for key: %d", key);
        }

        return;
    }

    // Handle regular settings
    uint8_t value = variable_item_get_current_value_index(item);
    FURI_LOG_D("SettingsChange", "Attempting to set setting: key=%d, value=%d", key, value);

    if(settings_set(context->settings, key, value, context)) {
        variable_item_set_current_value_text(item, metadata->data.setting.value_names[value]);
        if(key == SETTING_RGB_MODE && context->send_uart_command) {
            const char* modes[] = {"stealth", "normal", "rainbow"};
            const char* mode = "normal";
            if(value < (sizeof(modes) / sizeof(modes[0]))) mode = modes[value];
            char command[64];
            snprintf(command, sizeof(command), "setrgbmode %s\n", mode);
            context->send_uart_command(command, context->context);
        } else if(metadata->data.setting.uart_command && context->send_uart_command) {
            char command[64];
            snprintf(
                command,
                sizeof(command),
                "%s %d\n",
                metadata->data.setting.uart_command,
                value + 1);
            FURI_LOG_D("SettingsChange", "Sending UART command: %s", command);
            context->send_uart_command(command, context->context);
        }
    }
}

static void settings_menu_callback(void* context, uint32_t index) {
    UNUSED(index);
    AppState* app_state = context;
    view_dispatcher_switch_to_view(app_state->view_dispatcher, VIEW_SETTINGS);
    app_state->current_view = VIEW_SETTINGS;
}

static void settings_action_callback(void* context, uint32_t index) {
    SettingsUIContext* settings_context = context;
    // Execute the action directly without value simulation
    settings_set(settings_context->settings, index, 0, settings_context);
}

// submenu callback to open wifi hardware settings menu from settings actions
#define WIFI_SETTINGS_MENU_ID 200
#define STATUS_IDLE_MENU_ID   201
static void wifi_settings_menu_callback(void* context, uint32_t index) {
    UNUSED(index);
    AppState* state = (AppState*)context;
    if(!state) return;
    show_wifi_settings_menu(state);
    state->came_from_settings = true;
}

static void status_idle_menu_callback(void* context, uint32_t index) {
    UNUSED(index);
    AppState* state = (AppState*)context;
    if(!state) return;
    show_status_idle_menu(state);
    state->came_from_settings = true;
}

void settings_setup_gui(VariableItemList* list, SettingsUIContext* context) {
    FURI_LOG_D("SettingsSetup", "Entering settings_setup_gui");
    AppState* app_state = (AppState*)context->context;

    // Add "Configuration" submenu item
    submenu_add_item(
        app_state->settings_actions_menu,
        "Configuration  >",
        SETTINGS_COUNT,
        settings_menu_callback,
        app_state);

    // add hardware submenu item in settings actions menu
    submenu_add_item(
        app_state->settings_actions_menu,
        "Hardware  >",
        WIFI_SETTINGS_MENU_ID,
        wifi_settings_menu_callback,
        app_state);

    // add status idle animation submenu item
    submenu_add_item(
        app_state->settings_actions_menu,
        "Status display animation  >",
        STATUS_IDLE_MENU_ID,
        status_idle_menu_callback,
        app_state);

    // Iterate over all settings
    for(SettingKey key = 0; key < SETTINGS_COUNT; key++) {
        // Skip hidden settings
        if(!setting_is_visible(key)) {
            continue;
        }

        const SettingMetadata* metadata = settings_get_metadata(key);
        if(!metadata) continue;

        if(metadata->is_action) {
            // Add all action items to the actions submenu
            submenu_add_item(
                app_state->settings_actions_menu,
                metadata->name,
                key,
                settings_action_callback,
                context);
            FURI_LOG_D("SettingsSetup", "Added action button: %s", metadata->name);
        } else {
            // Handle regular settings
            // Use statically allocated context from SettingsUIContext
            VariableItemContext* item_context = &context->item_contexts[key];
            item_context->settings_ui_context = context;
            item_context->key = key;

            VariableItem* item = variable_item_list_add(
                list,
                metadata->name,
                metadata->data.setting.max_value + 1,
                settings_item_change_callback,
                item_context);

            if(item) {
                uint8_t current_value = settings_get(context->settings, key);
                variable_item_set_current_value_index(item, current_value);
                variable_item_set_current_value_text(
                    item, metadata->data.setting.value_names[current_value]);
                FURI_LOG_D("SettingsSetup", "Added setting item: %s", metadata->name);
            }
        }
    }
}

bool settings_custom_event_callback(void* context, uint32_t event_id) {
    AppState* app_state = (AppState*)context;
    if(!app_state) return false;

    // Skip processing events if the dispatcher is no longer running
    // This helps prevent crashes during shutdown
    if(!app_state->view_dispatcher) {
        FURI_LOG_I("Settings", "Ignoring custom event during shutdown: %lu", event_id);
        return false;
    }

    switch(event_id) {
    case SETTING_CLEAR_LOGS:
        show_confirmation_dialog_ex(
            app_state,
            "Clear Logs",
            "Clear all log files?\n"
            "This cannot be undone.\n"
            "Files located at:\n"
            "apps_data/ghost_esp/logs\n",
            logs_clear_confirmed_callback,
            logs_clear_cancelled_callback);
        return true;

    case SETTING_CLEAR_PCAPS:
        show_confirmation_dialog_ex(
            app_state,
            "Clear PCAPs",
            "Clear all PCAP files?\n"
            "This cannot be undone.\n"
            "Files located at:\n"
            "apps_data/ghost_esp/pcaps\n",
            pcap_clear_confirmed_callback,
            pcap_clear_cancelled_callback);
        return true;

    case SETTING_CLEAR_WARDRIVE:
        show_confirmation_dialog_ex(
            app_state,
            "Clear Wardrives",
            "Clear all wardrive files?\n"
            "This cannot be undone.\n"
            "Files located at:\n"
            "apps_data/ghost_esp/wardrive\n",
            wardrive_clear_confirmed_callback,
            wardrive_clear_cancelled_callback);
        return true;

    case SETTING_CLEAR_NVS:
        show_confirmation_dialog_ex(
            app_state,
            "Clear NVS",
            "Clear NVS settings?\n"
            "This will reset all ESP\n"
            "settings to default.\n"
            "This cannot be undone.",
            nvs_clear_confirmed_callback,
            nvs_clear_cancelled_callback);
        return true;

    case SETTING_SHOW_INFO: {
        // Create a new context for the confirmation dialog
        SettingsConfirmContext* confirm_ctx = malloc(sizeof(SettingsConfirmContext));
        if(!confirm_ctx) return false;
        confirm_ctx->state = app_state;
        app_state->active_confirm_context = confirm_ctx;

        const char* info_text = "Created by: Spooky\n"
                                "Updated by:\n"
                                "@jaylikesbunda\n"
                                "@tototo31\n"
                                "Built with <3\n"
                                "GhostESP-Revival/GhostESP-FlipperCompanion\n\n";

        confirmation_view_set_header(app_state->confirmation_view, "Ghost ESP v1.7.0");
        confirmation_view_set_text(app_state->confirmation_view, info_text);

        // Save current view before switching
        app_state->previous_view = app_state->current_view;

        confirmation_view_set_ok_callback(
            app_state->confirmation_view, app_info_ok_callback, confirm_ctx);
        confirmation_view_set_cancel_callback(
            app_state->confirmation_view, app_info_cancel_callback, confirm_ctx);

        view_dispatcher_switch_to_view(app_state->view_dispatcher, VIEW_CONFIRMATION);
        app_state->current_view = VIEW_CONFIRMATION;
        return true;
    }

    default:
        return false;
    }
}

// 6675636B796F7564656B69
