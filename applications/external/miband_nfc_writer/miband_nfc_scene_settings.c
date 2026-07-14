/**
 * @file miband_nfc_scene_settings.c
 * @brief Settings menu with persistent storage
 */

#include "miband_nfc_i.h"

#define TAG            "MiBandNfc"
#define SETTINGS_PATH  EXT_PATH("apps_data/miband_nfc/settings.bin")
#define SETTINGS_MAGIC 0x4D424E43 // "MBNC"

typedef struct {
    uint32_t magic;
    bool auto_backup_enabled;
    bool verify_after_write;
    bool show_detailed_progress;
    bool enable_logging;
} MiBandSettings;

bool miband_settings_save(MiBandNfcApp* app) {
    if(!app || !app->storage) {
        FURI_LOG_E(TAG, "Invalid app or storage");
        return false;
    }

    Storage* storage = app->storage;

    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/miband_nfc"));

    MiBandSettings settings = {
        .magic = SETTINGS_MAGIC,
        .auto_backup_enabled = app->auto_backup_enabled,
        .verify_after_write = app->verify_after_write,
        .show_detailed_progress = app->show_detailed_progress,
        .enable_logging = app->enable_logging,
    };

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to alloc file for save");
        return false;
    }

    bool success = false;

    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        success = storage_file_write(file, &settings, sizeof(MiBandSettings)) ==
                  sizeof(MiBandSettings);
        storage_file_close(file);
        FURI_LOG_I(TAG, "Settings saved: %s", success ? "OK" : "FAIL");
    } else {
        FURI_LOG_E(TAG, "Failed to open settings file for writing");
    }

    storage_file_free(file);
    return success;
}

bool miband_settings_load(MiBandNfcApp* app) {
    if(!app || !app->storage) {
        FURI_LOG_E(TAG, "Invalid app or storage");
        return false;
    }
    Storage* storage = app->storage;
    MiBandSettings settings;
    bool success = false;

    File* file = storage_file_alloc(storage);
    if(!file) {
        FURI_LOG_E(TAG, "Failed to alloc file");
        return false;
    }
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &settings, sizeof(MiBandSettings)) == sizeof(MiBandSettings)) {
            if(settings.magic == SETTINGS_MAGIC) {
                app->auto_backup_enabled = settings.auto_backup_enabled;
                app->verify_after_write = settings.verify_after_write;
                app->show_detailed_progress = settings.show_detailed_progress;
                app->enable_logging = settings.enable_logging;
                success = true;
                FURI_LOG_I(TAG, "Settings loaded successfully");
            } else {
                FURI_LOG_W(TAG, "Invalid settings magic");
            }
        }
        storage_file_close(file);
    } else {
        FURI_LOG_I(TAG, "No settings file found, using defaults");
    }

    storage_file_free(file);
    return success;
}

void settings_submenu_callback(void* context, uint32_t index) {
    MiBandNfcApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// FIX: Static buffers for submenu labels - submenu stores pointers, NOT copies.
// Allocating FuriStrings and freeing them after submenu_add_item caused use-after-free.
static char settings_label_backup[32];
static char settings_label_verify[32];
static char settings_label_progress[32];
static char settings_label_logging[32];
static char settings_label_export[32];

void miband_nfc_scene_settings_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");

    // Auto Backup toggle
    snprintf(
        settings_label_backup,
        sizeof(settings_label_backup),
        "Auto Backup: %s",
        app->auto_backup_enabled ? "ON" : "OFF");
    submenu_add_item(
        app->submenu,
        settings_label_backup,
        SettingsIndexAutoBackup,
        settings_submenu_callback,
        app);

    // Verify after write toggle
    snprintf(
        settings_label_verify,
        sizeof(settings_label_verify),
        "Verify After Write: %s",
        app->verify_after_write ? "ON" : "OFF");
    submenu_add_item(
        app->submenu,
        settings_label_verify,
        SettingsIndexVerifyAfterWrite,
        settings_submenu_callback,
        app);

    // Show detailed progress toggle
    snprintf(
        settings_label_progress,
        sizeof(settings_label_progress),
        "Detailed Progress: %s",
        app->show_detailed_progress ? "ON" : "OFF");
    submenu_add_item(
        app->submenu,
        settings_label_progress,
        SettingsIndexShowProgress,
        settings_submenu_callback,
        app);

    // Enable logging toggle
    snprintf(
        settings_label_logging,
        sizeof(settings_label_logging),
        "Enable Logging: %s",
        app->enable_logging ? "ON" : "OFF");
    submenu_add_item(
        app->submenu,
        settings_label_logging,
        SettingsIndexEnableLogging,
        settings_submenu_callback,
        app);

    // Export logs
    size_t log_count = miband_logger_get_count(app->logger);
    snprintf(
        settings_label_export,
        sizeof(settings_label_export),
        "Export Logs (%zu entries)",
        log_count);
    submenu_add_item(
        app->submenu,
        settings_label_export,
        SettingsIndexExportLogs,
        settings_submenu_callback,
        app);

    // Clear logs
    submenu_add_item(
        app->submenu, "Clear Logs", SettingsIndexClearLogs, settings_submenu_callback, app);

    // Back button
    submenu_add_item(
        app->submenu, "< Back to Menu", SettingsIndexBack, settings_submenu_callback, app);

    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMainMenu);
}

bool miband_nfc_scene_settings_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SettingsIndexAutoBackup:
            app->auto_backup_enabled = !app->auto_backup_enabled;
            miband_settings_save(app);
            if(app->logger) {
                miband_logger_log(
                    app->logger,
                    LogLevelInfo,
                    "Auto backup %s",
                    app->auto_backup_enabled ? "enabled" : "disabled");
            }
            // Refresh scene
            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;

        case SettingsIndexVerifyAfterWrite:
            app->verify_after_write = !app->verify_after_write;
            miband_settings_save(app);
            if(app->logger) {
                miband_logger_log(
                    app->logger,
                    LogLevelInfo,
                    "Verify after write %s",
                    app->verify_after_write ? "enabled" : "disabled");
            }
            // Refresh scene
            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;

        case SettingsIndexShowProgress:
            app->show_detailed_progress = !app->show_detailed_progress;
            miband_settings_save(app);

            // Refresh scene
            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;

        case SettingsIndexEnableLogging:
            app->enable_logging = !app->enable_logging;
            if(app->logger) {
                miband_logger_log(
                    app->logger,
                    LogLevelInfo,
                    "Logging %s",
                    app->enable_logging ? "enabled" : "disabled");
            }
            miband_settings_save(app);
            miband_logger_set_enabled(app->logger, app->enable_logging);
            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;

        case SettingsIndexExportLogs: {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            FuriString* filename = furi_string_alloc_printf(
                "log_%04d%02d%02d_%02d%02d%02d.txt",
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute,
                dt.second);

            bool success = miband_logger_export(app->logger, furi_string_get_cstr(filename));
            furi_string_free(filename);

            if(app->logger) {
                miband_logger_log(app->logger, LogLevelInfo, "Log export requested");
            }
            popup_reset(app->popup);
            popup_set_header(
                app->popup, success ? "Success" : "Failed", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                success ? "Logs exported" : "Export failed",
                64,
                30,
                AlignCenter,
                AlignTop);
            view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
            furi_delay_ms(2000);

            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;
        }

        case SettingsIndexClearLogs:
            if(app->logger) {
                miband_logger_log(app->logger, LogLevelInfo, "Log clear requested");
            }
            miband_logger_clear(app->logger);
            miband_nfc_scene_settings_on_exit(app);
            miband_nfc_scene_settings_on_enter(app);
            consumed = true;
            break;

        case SettingsIndexBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void miband_nfc_scene_settings_on_exit(void* context) {
    MiBandNfcApp* app = context;
    submenu_reset(app->submenu);
}
