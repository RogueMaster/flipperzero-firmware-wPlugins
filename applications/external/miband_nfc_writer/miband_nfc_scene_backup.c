/**
 * @file miband_nfc_scene_backup.c
 * @brief Automatic backup before write operations
 * 
 * This scene automatically backs up the current Mi Band data before
 * performing write operations. Creates backups in /ext/nfc/backups/
 * with timestamp for easy restoration.
 */

#include "miband_nfc_i.h"
#include <datetime/datetime.h>

#define TAG           "MiBandNfc"
#define BACKUP_FOLDER EXT_PATH("nfc/backups")

/**
 * @brief Create backup directory if it doesn't exist
 * 
 * @param storage Storage API instance
 * @return true if directory exists or was created successfully
 */
static bool ensure_backup_directory(Storage* storage) {
    if(!storage_simply_mkdir(storage, BACKUP_FOLDER)) {
        // Check if it already exists
        FileInfo file_info;
        if(storage_common_stat(storage, BACKUP_FOLDER, &file_info) == FSE_OK) {
            return file_info.flags & FSF_DIRECTORY;
        }
        return false;
    }
    return true;
}

/**
 * @brief Read all data from Mi Band using multiple key strategies
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @return true if all sectors read successfully
 */
static bool backup_read_all_data(MiBandNfcApp* app) {
    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);

    popup_set_text(app->popup, "Reading Mi Band...\n\nSector 0/16", 64, 22, AlignCenter, AlignTop);

    // FIX: Use app->temp_text_buffer instead of local FuriString.
    // popup_set_text stores pointer only - local string freed = use-after-free.
    for(size_t sector = 0; sector < total_sectors; sector++) {
        uint8_t first_block = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);

        if(sector % 2 == 0) {
            furi_string_printf(
                app->temp_text_buffer, "Reading...\nSector %zu/%zu\n", sector + 1, total_sectors);

            uint32_t percent = ((sector + 1) * 100) / total_sectors;
            furi_string_cat_str(app->temp_text_buffer, "[");
            for(uint8_t i = 0; i < 20; i++) {
                if(i < (percent / 5))
                    furi_string_cat_str(app->temp_text_buffer, "=");
                else
                    furi_string_cat_str(app->temp_text_buffer, " ");
            }
            furi_string_cat_printf(app->temp_text_buffer, "]\n%lu%%", percent);

            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->temp_text_buffer),
                64,
                18,
                AlignCenter,
                AlignTop);
        }

        furi_delay_ms(50);

        MfClassicKey keys_to_try[3];
        MfClassicKeyType key_types[3];
        int keys_count = 0;

        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(app->mf_classic_data, sector);

        if(sec_tr && mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeA)) {
            memcpy(keys_to_try[keys_count].data, sec_tr->key_a.data, 6);
            key_types[keys_count] = MfClassicKeyTypeA;
            keys_count++;
        }

        if(sec_tr && mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeB)) {
            memcpy(keys_to_try[keys_count].data, sec_tr->key_b.data, 6);
            key_types[keys_count] = MfClassicKeyTypeB;
            keys_count++;
        }

        memset(keys_to_try[keys_count].data, 0xFF, 6);
        key_types[keys_count] = MfClassicKeyTypeA;
        keys_count++;

        bool sector_read = false;
        for(int key_idx = 0; key_idx < keys_count && !sector_read; key_idx++) {
            MfClassicAuthContext auth_context;
            MfClassicError error = mf_classic_poller_sync_auth(
                app->nfc, first_block, &keys_to_try[key_idx], key_types[key_idx], &auth_context);

            if(error == MfClassicErrorNone) {
                bool all_read = true;
                for(uint8_t block_in_sector = 0; block_in_sector < blocks_in_sector;
                    block_in_sector++) {
                    size_t block_idx = first_block + block_in_sector;

                    error = mf_classic_poller_sync_read_block(
                        app->nfc,
                        block_idx,
                        &keys_to_try[key_idx],
                        key_types[key_idx],
                        &app->target_data->block[block_idx]);

                    if(error != MfClassicErrorNone) {
                        all_read = false;
                        break;
                    }
                }

                if(all_read) {
                    sector_read = true;
                    FURI_BIT_SET(app->target_data->key_a_mask, sector);
                    FURI_BIT_SET(app->target_data->key_b_mask, sector);
                }
            }
        }

        if(!sector_read) {
            FURI_LOG_W(TAG, "Failed to read sector %zu for backup", sector);
            return false;
        }

        furi_delay_ms(50);
    }
    return true;
}

/**
 * @brief NFC scanner callback for card detection
 *
 * Called from NFC worker thread. Sends custom event if MfClassic card found.
 */
static void backup_scanner_callback(NfcScannerEvent event, void* context) {
    MiBandNfcApp* app = context;
    if(event.type == NfcScannerEventTypeDetected) {
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            if(event.data.protocols[i] == NfcProtocolMfClassic) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MiBandNfcCustomEventCardDetected);
                return;
            }
        }
    }
}

/**
 * @brief Execute backup read + save and proceed to writer
 *
 * Called after card is detected via async scanner.
 */
static void backup_execute_and_show_result(MiBandNfcApp* app) {
    if(!backup_read_all_data(app)) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Backup: cannot read all sectors");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Read Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot read\nall sectors", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    FuriString* backup_path = furi_string_alloc_printf(
        "%s/backup_%04d%02d%02d_%02d%02d%02d.nfc",
        BACKUP_FOLDER,
        datetime.year,
        datetime.month,
        datetime.day,
        datetime.hour,
        datetime.minute,
        datetime.second);

    popup_set_text(app->popup, "Saving backup...", 64, 11, AlignCenter, AlignTop);

    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->target_data);
    bool save_success = nfc_device_save(app->nfc_device, furi_string_get_cstr(backup_path));
    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->mf_classic_data);

    popup_reset(app->popup);

    if(save_success) {
        if(app->logger) {
            miband_logger_log(
                app->logger, LogLevelInfo, "Backup saved: %s", furi_string_get_cstr(backup_path));
        }
        notification_message(app->notifications, &sequence_success);
        popup_set_header(app->popup, "Backup Created!", 2, 2, AlignLeft, AlignTop);

        furi_string_printf(
            app->temp_text_buffer,
            "Saved:\n%04d/%02d/%02d\n%02d:%02d:%02d",
            datetime.year,
            datetime.month,
            datetime.day,
            datetime.hour,
            datetime.minute,
            datetime.second);

        popup_set_text(
            app->popup, furi_string_get_cstr(app->temp_text_buffer), 2, 16, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 68, 6, &I_DolphinSuccess_91x55);
        FURI_LOG_I(TAG, "Backup saved: %s", furi_string_get_cstr(backup_path));
    } else {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to save backup file");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Save Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot save\nbackup file", 64, 22, AlignCenter, AlignTop);
        FURI_LOG_E(TAG, "Failed to save backup");
    }

    furi_string_free(backup_path);
    furi_delay_ms(3000);

    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneWriter);
}

/**
 * @brief Scene entry point
 *
 * Sets up UI and starts async NFC scanner for card detection.
 * Back button works during the card waiting phase.
 *
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_backup_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Backup operation started");
    }
    popup_reset(app->popup);
    popup_set_header(app->popup, "Creating Backup", 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Place Mi Band near\nFlipper Zero...", 64, 22, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    if(!ensure_backup_directory(app->storage)) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Cannot create backup directory");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Backup Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot create\nbackup folder", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    mf_classic_reset(app->target_data);
    app->target_data->type = app->mf_classic_data->type;

    // Start async NFC scanner - event loop stays free, Back button works
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, backup_scanner_callback, app);
    app->is_scan_active = true;
}

/**
 * @brief Scene event handler
 *
 * Handles card detection (from async scanner) and Back button.
 */
bool miband_nfc_scene_backup_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiBandNfcCustomEventCardDetected && app->is_scan_active) {
            // Stop async scanner - card found
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }
            app->is_scan_active = false;

            // Perform sync backup operation
            backup_execute_and_show_result(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // User pressed Back - cancel card detection, return to previous scene
        if(app->scanner) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->scanner = NULL;
        }
        app->is_scan_active = false;
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Scene exit handler
 */
void miband_nfc_scene_backup_on_exit(void* context) {
    MiBandNfcApp* app = context;

    // Safety: stop scanner if still running
    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    app->is_scan_active = false;

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
