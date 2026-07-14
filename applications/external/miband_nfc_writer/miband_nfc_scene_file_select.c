/**
 * @file miband_nfc_scene_file_select.c
 * @brief File selection with loading feedback
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

void miband_nfc_scene_file_select_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    furi_string_set(app->file_path, NFC_APP_FOLDER);
    mf_classic_reset(app->mf_classic_data);

    if(!storage_simply_mkdir(app->storage, NFC_APP_FOLDER)) {
        FURI_LOG_W(TAG, "Failed to create NFC directory, continuing anyway");
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = NFC_APP_FOLDER;
    browser_options.hide_dot_files = true;
    browser_options.skip_assets = false;

    if(!dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options)) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelInfo, "File selection cancelled by user");
        }
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(app->logger) {
        miband_logger_log(
            app->logger, LogLevelInfo, "File selected: %s", furi_string_get_cstr(app->file_path));
    }

    FURI_LOG_D(TAG, "File selected: %s", furi_string_get_cstr(app->file_path));

    // Show loading popup
    popup_reset(app->popup);
    popup_set_header(app->popup, "Loading...", 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Reading file", 64, 30, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    furi_delay_ms(100); // Show loading message

    if(nfc_device_load(app->nfc_device, furi_string_get_cstr(app->file_path))) {
        FURI_LOG_D(TAG, "Loaded file: %s", furi_string_get_cstr(app->file_path));

        if(nfc_device_get_protocol(app->nfc_device) == NfcProtocolMfClassic) {
            const MfClassicData* loaded_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
            if(app->logger) {
                miband_logger_log(app->logger, LogLevelInfo, "MfClassic file loaded successfully");
            }
            if(loaded_data == NULL) {
                FURI_LOG_E(TAG, "Failed to get MfClassic data from loaded file");
                popup_set_text(app->popup, "Invalid data", 64, 30, AlignCenter, AlignTop);
                furi_delay_ms(1000);
                scene_manager_previous_scene(app->scene_manager);
                return;
            }

            mf_classic_copy(app->mf_classic_data, loaded_data);
            app->is_valid_nfc_data = true;

            popup_set_text(app->popup, "File loaded!", 64, 30, AlignCenter, AlignTop);
            furi_delay_ms(500);

            switch(app->current_operation) {
            case OperationTypeEmulateMagic:
                scene_manager_next_scene(app->scene_manager, MiBandNfcSceneMagicEmulator);
                break;
            case OperationTypeWriteOriginal:
                if(app->auto_backup_enabled) {
                    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneBackup);
                } else {
                    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneWriter);
                }
                break;
            case OperationTypeSaveMagic:
                scene_manager_next_scene(app->scene_manager, MiBandNfcSceneMagicSaver);
                break;
            case OperationTypeVerify:
                scene_manager_next_scene(app->scene_manager, MiBandNfcSceneVerify);
                break;
            default:
                scene_manager_previous_scene(app->scene_manager);
                break;
            }
        } else {
            FURI_LOG_W(TAG, "File is not Mifare Classic protocol.");
            if(app->logger) {
                miband_logger_log(app->logger, LogLevelWarning, "File is not MfClassic protocol");
            }
            popup_set_text(app->popup, "Not MF Classic", 64, 30, AlignCenter, AlignTop);
            furi_delay_ms(1000);
            scene_manager_previous_scene(app->scene_manager);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to load file: %s", furi_string_get_cstr(app->file_path));
        if(app->logger) {
            miband_logger_log(
                app->logger,
                LogLevelError,
                "Failed to load file: %s",
                furi_string_get_cstr(app->file_path));
        }
        popup_set_text(app->popup, "Load failed", 64, 30, AlignCenter, AlignTop);
        furi_delay_ms(1000);
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool miband_nfc_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(app->scene_manager);
    }
    return consumed;
}

void miband_nfc_scene_file_select_on_exit(void* context) {
    MiBandNfcApp* app = context;
    popup_reset(app->popup);
}
