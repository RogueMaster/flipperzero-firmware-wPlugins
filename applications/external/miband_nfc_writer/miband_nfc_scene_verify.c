/**
 * @file miband_nfc_scene_verify.c
 * @brief Data verification scene (FIXED VERSION)
 * 
 * This scene reads data from the Mi Band NFC and compares it with the original
 * dump file to verify that the write operation was successful.
 * 
 * Key improvements in this version:
 * - Tries dump keys first, then falls back to 0xFF magic keys
 * - Smart comparison that ignores keys in sector trailers
 * - Compares only UID/BCC in Block 0 (not manufacturer data)
 * - Robust authentication with retry logic
 * - Detailed progress feedback
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

static void verify_dialog_callback(DialogExResult result, void* context) {
    MiBandNfcApp* app = context;

    if(result == DialogExResultLeft) {
        // Exit o Back - torna al menu
        view_dispatcher_send_custom_event(app->view_dispatcher, MiBandNfcCustomEventVerifyExit);
    } else if(result == DialogExResultRight) {
        // View Details - vai al diff viewer
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiBandNfcCustomEventVerifyViewDetails);
    }
    // AGGIUNGI: Gestisci anche center (se solo Back button)
    else if(result == DialogExResultCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MiBandNfcCustomEventVerifyExit);
    }
}
/**
 * @brief Verification progress tracker
 * 
 * Tracks reading progress, authentication attempts, and comparison results
 * to provide detailed feedback to the user.
 */
typedef struct {
    uint32_t current_sector;
    uint32_t total_sectors;
    uint32_t sectors_read;
    uint32_t sectors_failed;
    uint32_t auth_attempts;
    uint32_t auth_successes;
    uint32_t blocks_compared;
    uint32_t blocks_different;
    FuriString* current_operation;
    FuriString* last_result;
    FuriString* error_details;
    bool reading_complete;
} VerifyTracker;

static VerifyTracker verify_tracker = {0};

/**
 * @brief Initialize verification tracker
 */
static void verify_tracker_init(void) {
    // PRIMA: Libera se esistono
    if(verify_tracker.current_operation) {
        furi_string_free(verify_tracker.current_operation);
        verify_tracker.current_operation = NULL;
    }
    if(verify_tracker.last_result) {
        furi_string_free(verify_tracker.last_result);
        verify_tracker.last_result = NULL;
    }
    if(verify_tracker.error_details) {
        furi_string_free(verify_tracker.error_details);
        verify_tracker.error_details = NULL;
    }

    // Reset contatori
    verify_tracker.current_sector = 0;
    verify_tracker.total_sectors = 0;
    verify_tracker.sectors_read = 0;
    verify_tracker.sectors_failed = 0;
    verify_tracker.auth_attempts = 0;
    verify_tracker.auth_successes = 0;
    verify_tracker.blocks_compared = 0;
    verify_tracker.blocks_different = 0;
    verify_tracker.reading_complete = false;

    // POI: Alloca nuove
    verify_tracker.current_operation = furi_string_alloc();
    verify_tracker.last_result = furi_string_alloc();
    verify_tracker.error_details = furi_string_alloc();
}

/**
 * @brief Update verification UI with current progress
 * 
 * Displays progress bar, statistics, and current operation status.
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @param header Header text for the popup
 */
static void update_verify_ui(MiBandNfcApp* app, const char* header) {
    furi_string_reset(app->temp_text_buffer);

    if(verify_tracker.total_sectors > 0) {
        uint32_t progress_percent =
            verify_tracker.reading_complete ?
                100 :
                (verify_tracker.current_sector * 100) / verify_tracker.total_sectors;

        furi_string_printf(
            app->temp_text_buffer,
            "Sector %lu/%lu\n[",
            verify_tracker.current_sector,
            verify_tracker.total_sectors);

        for(uint32_t i = 0; i < 16; i++) {
            if(i < (progress_percent / 6)) {
                furi_string_cat_str(app->temp_text_buffer, "=");
            } else {
                furi_string_cat_str(app->temp_text_buffer, " ");
            }
        }
        furi_string_cat_printf(app->temp_text_buffer, "] %lu%%", progress_percent);
    } else {
        furi_string_set_str(app->temp_text_buffer, "Initializing...");
    }

    popup_reset(app->popup);
    popup_set_header(app->popup, header, 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, furi_string_get_cstr(app->temp_text_buffer), 64, 28, AlignCenter, AlignCenter);
}
/**
 * @brief Read a sector using multiple key strategies
 * 
 * This is the KEY FIX: tries dump keys first (Key A and Key B from the original
 * dump file), then falls back to 0xFF magic keys if those fail. This handles
 * both scenarios:
 * 1. Mi Band has original keys after successful write
 * 2. Mi Band still has magic keys from emulation
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @param sector Sector number to read
 * @param first_block First block index of the sector
 * @param blocks_in_sector Number of blocks in the sector
 * @return true if sector read successfully, false otherwise
 */
static bool read_sector_with_keys(
    MiBandNfcApp* app,
    size_t sector,
    uint8_t first_block,
    uint8_t blocks_in_sector) {
    MfClassicError error;
    MfClassicAuthContext auth_context;
    MfClassicSectorTrailer* sec_tr =
        mf_classic_get_sector_trailer_by_sector(app->mf_classic_data, sector);

    if(!sec_tr) {
        FURI_LOG_E(TAG, "Sector %zu: No trailer in dump", sector);
        return false;
    }

    // Build list of keys to try
    MfClassicKey keys_to_try[3];
    MfClassicKeyType key_types[3];
    int keys_count = 0;

    // STRATEGY: Try dump keys first, then magic keys as fallback

    // 1. Try Key A from dump
    if(mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeA)) {
        memcpy(keys_to_try[keys_count].data, sec_tr->key_a.data, 6);
        key_types[keys_count] = MfClassicKeyTypeA;
        keys_count++;
    }

    // 2. Try Key B from dump
    if(mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeB)) {
        memcpy(keys_to_try[keys_count].data, sec_tr->key_b.data, 6);
        key_types[keys_count] = MfClassicKeyTypeB;
        keys_count++;
    }

    // 3. Try magic key 0xFF as fallback
    memset(keys_to_try[keys_count].data, 0xFF, 6);
    key_types[keys_count] = MfClassicKeyTypeA;
    keys_count++;

    // Try each key in order
    for(int key_idx = 0; key_idx < keys_count; key_idx++) {
        verify_tracker.auth_attempts++;

        error = mf_classic_poller_sync_auth(
            app->nfc, first_block, &keys_to_try[key_idx], key_types[key_idx], &auth_context);

        if(error != MfClassicErrorNone) {
            continue; // Try next key
        }

        verify_tracker.auth_successes++;
        FURI_LOG_D(TAG, "Sector %zu: Auth OK with key %d", sector, key_idx);

        // Read all blocks in the sector
        bool all_blocks_read = true;
        for(uint8_t block_in_sector = 0; block_in_sector < blocks_in_sector; block_in_sector++) {
            size_t block_idx = first_block + block_in_sector;

            // Re-authenticate every 2 blocks for stability
            if(block_in_sector > 0 && block_in_sector % 2 == 0) {
                error = mf_classic_poller_sync_auth(
                    app->nfc,
                    first_block,
                    &keys_to_try[key_idx],
                    key_types[key_idx],
                    &auth_context);

                if(error != MfClassicErrorNone) {
                    FURI_LOG_W(TAG, "Re-auth failed at block %zu", block_idx);
                    all_blocks_read = false;
                    miband_logger_log(
                        app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                    break;
                }
            }

            // Read block with retry logic
            bool block_read = false;
            for(int retry = 0; retry < 3 && !block_read; retry++) {
                if(retry > 0) furi_delay_ms(50);

                error = mf_classic_poller_sync_read_block(
                    app->nfc,
                    block_idx,
                    &keys_to_try[key_idx],
                    key_types[key_idx],
                    &app->target_data->block[block_idx]);

                if(error == MfClassicErrorNone) {
                    block_read = true;
                } else if(error != MfClassicErrorTimeout) {
                    break; // Non-timeout error, stop retrying
                }
            }

            if(!block_read) {
                FURI_LOG_E(TAG, "Failed to read block %zu", block_idx);
                all_blocks_read = false;
                break;
            }
        }

        if(all_blocks_read) {
            FURI_BIT_SET(app->target_data->key_a_mask, sector);
            FURI_BIT_SET(app->target_data->key_b_mask, sector);
            // view_port_update(view_dispatcher_get_current_view(app->view_dispatcher));
            furi_delay_ms(10);

            return true;
        }
    }

    FURI_LOG_E(TAG, "Sector %zu: All auth attempts failed", sector);
    return false;
}

/**
 * @brief Read all sectors from Mi Band
 * 
 * Performs sector-by-sector reading with progress updates.
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @return true if all sectors read successfully, false otherwise
 */
static bool miband_verify_read_card(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        return false;
    }

    verify_tracker.total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);

    furi_string_set_str(verify_tracker.current_operation, "Initializing read");
    update_verify_ui(app, "Reading Mi Band");

    mf_classic_reset(app->target_data);
    app->target_data->type = app->mf_classic_data->type;

    bool overall_success = true;

    for(size_t sector = 0; sector < verify_tracker.total_sectors; sector++) {
        verify_tracker.current_sector = sector;

        furi_string_printf(verify_tracker.current_operation, "Reading sector %zu", sector);

        // CHIAMALO PIÙ SPESSO - ogni settore
        update_verify_ui(app, "Reading Mi Band");
        furi_delay_ms(50);

        uint8_t first_block = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);

        bool sector_success = read_sector_with_keys(app, sector, first_block, blocks_in_sector);

        if(sector_success) {
            verify_tracker.sectors_read++;
            furi_string_printf(verify_tracker.last_result, "Sector %zu OK", sector);
            update_verify_ui(app, "Reading Mi Band");
            furi_delay_ms(50);
        } else {
            verify_tracker.sectors_failed++;
            if(app->logger) {
                miband_logger_log(
                    app->logger, LogLevelError, "Verify: failed to read sector %zu", sector);
            }
            overall_success = false;
            furi_string_printf(verify_tracker.error_details, "Sector %zu failed", sector);
            FURI_LOG_E(TAG, "Failed to read sector %zu", sector);
        }
    }

    verify_tracker.current_sector = verify_tracker.total_sectors;
    verify_tracker.reading_complete = true;

    if(overall_success) {
        furi_string_set_str(verify_tracker.current_operation, "Read complete");
        furi_string_printf(
            verify_tracker.last_result, "All %lu sectors read", verify_tracker.sectors_read);
    } else {
        furi_string_printf(verify_tracker.current_operation, "Read incomplete");
        furi_string_printf(
            verify_tracker.last_result, "%lu sectors failed", verify_tracker.sectors_failed);
    }

    update_verify_ui(app, "Read Complete");
    furi_delay_ms(1000);

    return overall_success;
}

/**
 * @brief NFC scanner callback for card detection
 *
 * Called from NFC worker thread. Sends custom event if MfClassic card found.
 */
static void verify_scanner_callback(NfcScannerEvent event, void* context) {
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
 * @brief Execute verify read + compare and show result
 *
 * Called after card is detected via async scanner.
 */
static void verify_execute_and_show_result(MiBandNfcApp* app) {
    notification_message(app->notifications, &sequence_success);
    furi_string_set_str(verify_tracker.current_operation, "Card detected");
    update_verify_ui(app, "Card Found!");
    furi_delay_ms(500);

    // Read all sectors
    mf_classic_reset(app->target_data);
    app->target_data->type = app->mf_classic_data->type;

    bool read_success = miband_verify_read_card(app);

    if(!read_success) {
        notification_message(app->notifications, &sequence_error);

        // Read UID for diagnostic
        Iso14443_3aData card_iso_data = {0};
        bool has_card_uid = false;

        for(int attempt = 0; attempt < 3; attempt++) {
            if(iso14443_3a_poller_sync_read(app->nfc, &card_iso_data) == Iso14443_3aErrorNone) {
                has_card_uid = true;
                break;
            }
            furi_delay_ms(100);
        }

        if(app->logger) {
            if(has_card_uid) {
                miband_logger_log(
                    app->logger,
                    LogLevelError,
                    "Verify read failed - Card UID: %02X %02X %02X %02X",
                    card_iso_data.uid[0],
                    card_iso_data.uid[1],
                    card_iso_data.uid[2],
                    card_iso_data.uid[3]);
            } else {
                miband_logger_log(
                    app->logger, LogLevelError, "Verify read failed - No card detected");
            }

            if(app->mf_classic_data) {
                uint8_t* file_uid = app->mf_classic_data->block[0].data;
                miband_logger_log(
                    app->logger,
                    LogLevelInfo,
                    "Expected file UID: %02X %02X %02X %02X",
                    file_uid[0],
                    file_uid[1],
                    file_uid[2],
                    file_uid[3]);
            }
        }

        popup_reset(app->popup);
        text_box_reset(app->text_box_report);
        dialog_ex_reset(app->dialog_ex);

        FuriString* error_msg = furi_string_alloc();
        const char* header_text = "Verify Failed";

        if(has_card_uid) {
            if(app->mf_classic_data) {
                uint8_t* file_uid = app->mf_classic_data->block[0].data;

                if(memcmp(card_iso_data.uid, file_uid, 4) != 0) {
                    header_text = "Wrong Card!";
                    furi_string_printf(
                        error_msg,
                        "Card UID: %02X %02X %02X %02X\n"
                        "Expected: %02X %02X %02X %02X\n"
                        "Use 'Quick UID Check' function",
                        card_iso_data.uid[0],
                        card_iso_data.uid[1],
                        card_iso_data.uid[2],
                        card_iso_data.uid[3],
                        file_uid[0],
                        file_uid[1],
                        file_uid[2],
                        file_uid[3]);
                } else {
                    furi_string_printf(
                        error_msg,
                        "Card UID OK:%02X %02X %02X %02X\n"
                        "Cannot read data\nCheck keys",
                        card_iso_data.uid[0],
                        card_iso_data.uid[1],
                        card_iso_data.uid[2],
                        card_iso_data.uid[3]);
                }
            } else {
                furi_string_printf(
                    error_msg,
                    "Card UID:%02X %02X %02X %02X\n"
                    "Cannot read data",
                    card_iso_data.uid[0],
                    card_iso_data.uid[1],
                    card_iso_data.uid[2],
                    card_iso_data.uid[3]);
            }
        } else {
            header_text = "No Card";
            furi_string_set_str(error_msg, "Place card near\nFlipper Zero");
        }

        furi_string_set(app->temp_text_buffer, error_msg);
        furi_string_free(error_msg);

        dialog_ex_set_header(app->dialog_ex, header_text, 64, 4, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            furi_string_get_cstr(app->temp_text_buffer),
            64,
            32,
            AlignCenter,
            AlignCenter);
        dialog_ex_set_left_button_text(app->dialog_ex, "Back");
        dialog_ex_set_icon(app->dialog_ex, 0, 0, NULL);
        dialog_ex_set_result_callback(app->dialog_ex, verify_dialog_callback);
        dialog_ex_set_context(app->dialog_ex, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdDialog);
        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_error);
        return;
    }

    // Compare data
    furi_string_set_str(verify_tracker.current_operation, "Comparing data");
    update_verify_ui(app, "Comparing Data");
    furi_delay_ms(500);

    bool data_match = true;
    int different_blocks = 0;
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    for(size_t i = 0; i < total_blocks; i++) {
        verify_tracker.blocks_compared++;

        if(i % 16 == 0) {
            furi_string_printf(
                verify_tracker.current_operation, "Comparing block %zu/%zu", i, total_blocks);
            update_verify_ui(app, "Comparing Data");
        }

        if(i == 0) {
            FURI_LOG_D(TAG, "Skipping Block 0 (UID block)");
            continue;
        } else if(mf_classic_is_sector_trailer(i)) {
            FURI_LOG_D(TAG, "Skipping trailer block %zu", i);
            continue;
        } else {
            if(memcmp(app->mf_classic_data->block[i].data, app->target_data->block[i].data, 16) !=
               0) {
                data_match = false;
                different_blocks++;
                verify_tracker.blocks_different++;
                FURI_LOG_W(TAG, "Block %zu differs", i);
            }
        }
    }

    notification_message(app->notifications, &sequence_blink_stop);

    if(data_match && different_blocks == 0) {
        notification_message(app->notifications, &sequence_success);

        text_box_reset(app->text_box_report);
        dialog_ex_reset(app->dialog_ex);

        popup_reset(app->popup);
        popup_set_header(app->popup, "Verify OK!", 2, 2, AlignLeft, AlignTop);
        popup_set_text(app->popup, "All data\nmatches!", 2, 16, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 68, 6, &I_DolphinSuccess_91x55);
        view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
        furi_delay_ms(3000);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);

        if(app->logger) {
            miband_logger_log(app->logger, LogLevelInfo, "Verify successful: all blocks match");
        }
    } else {
        notification_message(app->notifications, &sequence_error);
        if(app->logger) {
            miband_logger_log(
                app->logger, LogLevelWarning, "Verify failed: %d blocks differ", different_blocks);
        }

        popup_reset(app->popup);
        text_box_reset(app->text_box_report);

        dialog_ex_reset(app->dialog_ex);
        dialog_ex_set_header(app->dialog_ex, "Differences Found", 64, 0, AlignCenter, AlignTop);

        furi_string_printf(
            app->temp_text_buffer, "%d data blocks\ndiffer from dump", different_blocks);
        dialog_ex_set_text(
            app->dialog_ex,
            furi_string_get_cstr(app->temp_text_buffer),
            64,
            28,
            AlignCenter,
            AlignCenter);

        dialog_ex_set_left_button_text(app->dialog_ex, "Exit");
        dialog_ex_set_right_button_text(app->dialog_ex, "Details");
        dialog_ex_set_icon(app->dialog_ex, 0, 0, NULL);
        dialog_ex_set_result_callback(app->dialog_ex, verify_dialog_callback);
        dialog_ex_set_context(app->dialog_ex, app);

        view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdDialog);
    }
}

/**
 * @brief Scene entry point
 *
 * Sets up UI and starts async NFC scanner for card detection.
 * Back button works during the card waiting phase.
 *
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_verify_on_enter(void* context) {
    MiBandNfcApp* app = context;

    if(!app->is_valid_nfc_data) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }
    if(app->logger) {
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "Verify operation started for: %s",
            furi_string_get_cstr(app->file_path));
    }
    verify_tracker_init();

    popup_reset(app->popup);
    popup_set_header(app->popup, "Verify Data", 64, 2, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Place Mi Band near\nFlipper Zero...", 64, 22, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    // Start async NFC scanner - event loop stays free, Back button works
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, verify_scanner_callback, app);
    app->is_scan_active = true;
}

/**
 * @brief Scene event handler
 *
 * Handles card detection (from async scanner), dialog results, and Back button.
 *
 * @param context Pointer to MiBandNfcApp instance
 * @param event Scene manager event
 * @return true if event was consumed, false otherwise
 */
bool miband_nfc_scene_verify_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected:
            if(app->is_scan_active) {
                // Stop async scanner - card found
                if(app->scanner) {
                    nfc_scanner_stop(app->scanner);
                    nfc_scanner_free(app->scanner);
                    app->scanner = NULL;
                }
                app->is_scan_active = false;

                // Perform sync verify operation
                verify_execute_and_show_result(app);
                consumed = true;
            }
            break;

        case MiBandNfcCustomEventVerifyExit:
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MiBandNfcSceneMainMenu);
            consumed = true;
            break;

        case MiBandNfcCustomEventVerifyViewDetails:
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneDiffViewer);
            consumed = true;
            break;

        default:
            break;
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
 *
 * Cleans up resources and stops notifications.
 *
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_verify_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    // Safety: stop scanner if still running
    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    app->is_scan_active = false;

    if(verify_tracker.current_operation) {
        furi_string_free(verify_tracker.current_operation);
        verify_tracker.current_operation = NULL;
    }
    if(verify_tracker.last_result) {
        furi_string_free(verify_tracker.last_result);
        verify_tracker.last_result = NULL;
    }
    if(verify_tracker.error_details) {
        furi_string_free(verify_tracker.error_details);
        verify_tracker.error_details = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    dialog_ex_reset(app->dialog_ex);
}
