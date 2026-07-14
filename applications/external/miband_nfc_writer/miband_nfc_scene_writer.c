/**
 * @file miband_nfc_scene_writer.c
 * @brief Mi Band NFC data writing scene
 * 
 * This scene handles writing dump data to the Mi Band NFC chip. It implements
 * a robust writing strategy that:
 * 1. Detects the current state of the Mi Band (magic keys vs original keys)
 * 2. Preserves the UID in Block 0 (which is read-only on most cards)
 * 3. Writes all data blocks and sector trailers
 * 4. Uses appropriate authentication keys (0xFF for magic cards, dump keys for rewrites)
 * 5. Provides real-time progress feedback
 */

#include "miband_nfc_i.h"
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define TAG "MiBandNfc"

/**
 * @brief Write all blocks in a sector (excluding Block 0)
 * 
 * Helper function that writes all data blocks and the sector trailer
 * for a given sector. Uses retry logic for reliability.
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @param sector Sector number to write
 * @param first_block First block index of the sector
 * @param blocks_in_sector Number of blocks in the sector
 * @param auth_key Authentication key to use
 * @param key_type Key type (A or B)
 * @return true if all blocks written successfully, false otherwise
 */
static bool write_sector_blocks(
    MiBandNfcApp* app,
    size_t sector,
    uint8_t first_block,
    uint8_t blocks_in_sector,
    MfClassicKey* auth_key,
    MfClassicKeyType key_type) {
    MfClassicError error;
    MfClassicAuthContext auth_context;

    // Write all data blocks (excluding the trailer)
    for(uint8_t block_in_sector = 0; block_in_sector < (blocks_in_sector - 1); block_in_sector++) {
        size_t block_idx = first_block + block_in_sector;

        // Re-authenticate before each block (except first)
        if(block_in_sector > 0) {
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, auth_key, key_type, &auth_context);
            if(error != MfClassicErrorNone) {
                FURI_LOG_E(TAG, "Re-auth failed for block %zu", block_idx);
                miband_logger_log(app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                return false;
            }
        }

        // Write block with retry logic (up to 3 attempts)
        bool block_written = false;
        for(int retry = 0; retry < 3 && !block_written; retry++) {
            if(retry > 0) furi_delay_ms(100); // Delay between retries

            error = mf_classic_poller_sync_write_block(
                app->nfc, block_idx, auth_key, key_type, &app->mf_classic_data->block[block_idx]);

            if(error == MfClassicErrorNone) {
                block_written = true;
            } else if(error != MfClassicErrorTimeout) {
                break; // Non-timeout error, stop retrying
            }
        }

        if(!block_written) {
            FURI_LOG_E(TAG, "Failed to write block %zu", block_idx);
            return false;
        }
    }

    // Write the sector trailer
    size_t trailer_idx = first_block + blocks_in_sector - 1;
    bool trailer_written = false;

    for(int retry = 0; retry < 3 && !trailer_written; retry++) {
        if(retry > 0) {
            furi_delay_ms(100);
            // Re-authenticate before retry
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, auth_key, key_type, &auth_context);
            if(error != MfClassicErrorNone) {
                FURI_LOG_E(TAG, "Re-auth failed for trailer");
                miband_logger_log(app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                return false;
            }
        }

        error = mf_classic_poller_sync_write_block(
            app->nfc, trailer_idx, auth_key, key_type, &app->mf_classic_data->block[trailer_idx]);

        if(error == MfClassicErrorNone) {
            trailer_written = true;
        } else if(error != MfClassicErrorTimeout) {
            break; // Non-timeout error, stop retrying
        }
    }

    if(!trailer_written) {
        FURI_LOG_E(TAG, "Failed to write trailer at block %zu", trailer_idx);
        return false;
    }

    FURI_LOG_I(TAG, "Sector %zu: Write SUCCESS", sector);
    return true;
}

/**
 * @brief Main write function using synchronous approach - FIXED VERSION
 * 
 * This function implements a robust writing strategy:
 * 1. Detects card state (magic keys or original keys)
 * 2. Preserves Block 0 UID
 * 3. Writes all sectors with appropriate authentication
 * 4. Provides progress feedback
 * 5. FIXED: Properly handles authentication with 0xFF keys for first write
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @return true if write successful, false otherwise
 */
static bool miband_write_with_sync_approach(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        FURI_LOG_E(TAG, "No valid NFC data to write");
        return false;
    }

    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Starting sync write approach");
    }

    popup_set_text(app->popup, "Detecting card state...", 64, 24, AlignCenter, AlignTop);
    furi_delay_ms(100);

    MfClassicError error = MfClassicErrorNone;
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);
    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);

    FURI_LOG_I(
        TAG, "Starting sync write for %zu blocks in %zu sectors", total_blocks, total_sectors);

    FuriString* progress_msg = furi_string_alloc();
    bool should_break = false;
    bool write_success = true;

    // 1. Detect card type
    MfClassicType type = MfClassicType1k;
    MfClassicError detect_error = mf_classic_poller_sync_detect_type(app->nfc, &type);
    if(detect_error != MfClassicErrorNone) {
        FURI_LOG_E(TAG, "Card detection failed before write: %d", detect_error);
        furi_string_free(progress_msg);
        popup_set_text(
            app->popup, "Card not detected\nCheck position", 64, 24, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        return false;
    }

    FURI_LOG_I(TAG, "Card detected successfully, type: %d", type);

    // 2. Detect if card has magic keys (0xFF) or original keys
    bool has_magic_keys = false;
    MfClassicKey test_key = {0};
    memset(test_key.data, 0xFF, sizeof(test_key.data));
    MfClassicAuthContext test_auth;

    // Try to authenticate to sector 1 (block 4) with 0xFF keys
    MfClassicError test_error =
        mf_classic_poller_sync_auth(app->nfc, 4, &test_key, MfClassicKeyTypeA, &test_auth);

    if(test_error == MfClassicErrorNone) {
        has_magic_keys = true;
        FURI_LOG_I(TAG, "Mi Band has 0xFF magic keys - first write scenario");
        popup_set_text(
            app->popup, "Magic keys detected\nWriting...", 64, 24, AlignCenter, AlignTop);
    } else {
        FURI_LOG_I(TAG, "Mi Band has original keys - rewrite scenario");
        popup_set_text(
            app->popup, "Original keys detected\nRewriting...", 64, 24, AlignCenter, AlignTop);
    }

    furi_delay_ms(500);

    // 3. Write all sectors
    for(size_t sector = 0; sector < total_sectors && !should_break; sector++) {
        FURI_LOG_I(TAG, "Processing sector %zu...", sector);

        // Update progress display
        furi_string_reset(progress_msg);
        furi_string_printf(progress_msg, "Sector %zu/%zu\n\n", sector + 1, total_sectors);

        uint32_t progress_percent = total_sectors > 0 ? ((sector + 1) * 100) / total_sectors : 0;

        furi_string_cat_str(progress_msg, "[");
        for(uint8_t i = 0; i < 20; i++) {
            if(i < (progress_percent / 5)) {
                furi_string_cat_str(progress_msg, "=");
            } else if(i == (progress_percent / 5)) {
                furi_string_cat_str(progress_msg, ">");
            } else {
                furi_string_cat_str(progress_msg, " ");
            }
        }
        furi_string_cat_printf(progress_msg, "]\n%lu%%", progress_percent);

        popup_set_text(
            app->popup, furi_string_get_cstr(progress_msg), 64, 18, AlignCenter, AlignTop);
        furi_delay_ms(50);

        uint8_t first_block = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);
        bool sector_written = false;

        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(app->mf_classic_data, sector);

        MfClassicKey auth_key = {0};
        MfClassicKeyType auth_key_type = MfClassicKeyTypeA;
        MfClassicAuthContext auth_context;

        // 3.1 Special handling for Sector 0 (UID block)
        if(sector == 0) {
            FURI_LOG_I(TAG, "Sector 0: Special handling - preserving UID");

            MfClassicBlock current_block0;
            bool block0_read = false;
            MfClassicKey auth_key_uid = {0};

            // Try to read current Block 0 with 0xFF keys
            memset(auth_key_uid.data, 0xFF, sizeof(auth_key_uid.data));
            error = mf_classic_poller_sync_auth(
                app->nfc, 0, &auth_key_uid, MfClassicKeyTypeA, &auth_context);

            if(error == MfClassicErrorNone) {
                error = mf_classic_poller_sync_read_block(
                    app->nfc, 0, &auth_key_uid, MfClassicKeyTypeA, &current_block0);
                if(error == MfClassicErrorNone) {
                    block0_read = true;
                    auth_key_type = MfClassicKeyTypeA;
                    memcpy(&auth_key, &auth_key_uid, sizeof(MfClassicKey));
                    FURI_LOG_I(TAG, "Block 0 read with 0xFF keys");
                }
            }

            // If 0xFF fails, try with dump Key A
            if(!block0_read &&
               mf_classic_is_key_found(app->mf_classic_data, 0, MfClassicKeyTypeA)) {
                memcpy(auth_key.data, sec_tr->key_a.data, sizeof(auth_key.data));
                error = mf_classic_poller_sync_auth(
                    app->nfc, 0, &auth_key, MfClassicKeyTypeA, &auth_context);

                if(error == MfClassicErrorNone) {
                    error = mf_classic_poller_sync_read_block(
                        app->nfc, 0, &auth_key, MfClassicKeyTypeA, &current_block0);
                    if(error == MfClassicErrorNone) {
                        block0_read = true;
                        auth_key_type = MfClassicKeyTypeA;
                        FURI_LOG_I(TAG, "Block 0 read with dump Key A");
                    }
                }
            }

            // If Key A fails, try with dump Key B
            if(!block0_read &&
               mf_classic_is_key_found(app->mf_classic_data, 0, MfClassicKeyTypeB)) {
                memcpy(auth_key.data, sec_tr->key_b.data, sizeof(auth_key.data));
                error = mf_classic_poller_sync_auth(
                    app->nfc, 0, &auth_key, MfClassicKeyTypeB, &auth_context);

                if(error == MfClassicErrorNone) {
                    error = mf_classic_poller_sync_read_block(
                        app->nfc, 0, &auth_key, MfClassicKeyTypeB, &current_block0);
                    if(error == MfClassicErrorNone) {
                        block0_read = true;
                        auth_key_type = MfClassicKeyTypeB;
                        FURI_LOG_I(TAG, "Block 0 read with dump Key B");
                    }
                }
            }

            if(!block0_read) {
                FURI_LOG_E(TAG, "CRITICAL: Cannot read Block 0!");
                write_success = false;
                break;
            }

            // Log UID for debugging
            FURI_LOG_I(
                TAG,
                "Block 0 UID preserved: %02X%02X%02X%02X",
                current_block0.data[0],
                current_block0.data[1],
                current_block0.data[2],
                current_block0.data[3]);

            // Write data blocks 1 and 2 of sector 0 (skip trailer block 3)
            for(uint8_t block_in_sector = 1; block_in_sector < (blocks_in_sector - 1);
                block_in_sector++) {
                size_t block_idx = first_block + block_in_sector;

                // Re-authenticate if needed
                if(block_in_sector > 1) {
                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, auth_key_type, &auth_context);
                    if(error != MfClassicErrorNone) {
                        FURI_LOG_E(TAG, "Re-auth failed for block %zu", block_idx);
                        write_success = false;
                        break;
                    }
                }

                // Write block with retry
                bool block_written = false;
                for(int retry = 0; retry < 3 && !block_written; retry++) {
                    if(retry > 0) furi_delay_ms(100);

                    error = mf_classic_poller_sync_write_block(
                        app->nfc,
                        block_idx,
                        &auth_key,
                        auth_key_type,
                        &app->mf_classic_data->block[block_idx]);

                    if(error == MfClassicErrorNone) {
                        block_written = true;
                        FURI_LOG_D(TAG, "Sector 0 Block %zu written (retry %d)", block_idx, retry);
                    } else if(error != MfClassicErrorTimeout) {
                        break;
                    }
                }

                if(!block_written) {
                    FURI_LOG_E(TAG, "Failed to write sector 0 block %zu", block_idx);
                    write_success = false;
                    break;
                }
            }

            // Write sector 0 trailer (block 3)
            if(write_success) {
                size_t trailer_idx = first_block + blocks_in_sector - 1;
                FURI_LOG_D(TAG, "Writing sector 0 trailer block %zu", trailer_idx);

                bool trailer_written = false;
                for(int retry = 0; retry < 3 && !trailer_written; retry++) {
                    if(retry > 0) {
                        furi_delay_ms(100);
                        error = mf_classic_poller_sync_auth(
                            app->nfc, first_block, &auth_key, auth_key_type, &auth_context);
                        if(error != MfClassicErrorNone) break;
                    }

                    error = mf_classic_poller_sync_write_block(
                        app->nfc,
                        trailer_idx,
                        &auth_key,
                        auth_key_type,
                        &app->mf_classic_data->block[trailer_idx]);

                    if(error == MfClassicErrorNone) {
                        trailer_written = true;
                        sector_written = true;
                        FURI_LOG_I(TAG, "Sector 0 trailer written (retry %d)", retry);
                    } else if(error != MfClassicErrorTimeout) {
                        break;
                    }
                }

                if(!trailer_written) {
                    FURI_LOG_E(TAG, "Failed to write sector 0 trailer");
                    write_success = false;
                }
            }

            continue; // Skip to next sector
        }

        // 3.2 Handle sectors 1-15
        FURI_LOG_D(
            TAG,
            "Sector %zu: first_block=%d, blocks_in_sector=%d",
            sector,
            first_block,
            blocks_in_sector);

        // STRATEGY FOR WRITING SECTORS:
        // 1. If card has magic keys (0xFF) -> use 0xFF to write
        // 2. If card has original keys -> use dump keys
        // 3. Fallback to 0xFF if everything fails

        // OPTION 1: Try with magic keys (0xFF) if detected
        if(has_magic_keys) {
            FURI_LOG_I(TAG, "Sector %zu: Trying 0xFF keys (magic card)", sector);

            memset(auth_key.data, 0xFF, sizeof(auth_key.data));
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, &auth_key, MfClassicKeyTypeA, &auth_context);

            if(error == MfClassicErrorNone) {
                FURI_LOG_I(TAG, "Sector %zu: Auth with 0xFF successful", sector);
                sector_written = write_sector_blocks(
                    app, sector, first_block, blocks_in_sector, &auth_key, MfClassicKeyTypeA);

                if(sector_written) {
                    FURI_LOG_I(TAG, "Sector %zu written with 0xFF keys", sector);
                    continue; // Move to next sector
                }
            } else {
                FURI_LOG_W(TAG, "Sector %zu: Auth with 0xFF failed: %d", sector, error);
            }
        }

        // OPTION 2: Try with dump keys (for rewrites)
        if(!sector_written) {
            FURI_LOG_I(TAG, "Sector %zu: Trying dump keys", sector);

            // Try Key A from dump
            if(mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeA)) {
                memcpy(auth_key.data, sec_tr->key_a.data, sizeof(auth_key.data));
                auth_key_type = MfClassicKeyTypeA;

                for(int retry = 0; retry < 2 && !sector_written; retry++) {
                    if(retry > 0) {
                        FURI_LOG_D(TAG, "Retry %d with dump Key A", retry);
                        furi_delay_ms(200);
                    }

                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, auth_key_type, &auth_context);

                    if(error == MfClassicErrorNone) {
                        FURI_LOG_I(TAG, "Sector %zu: Auth with dump Key A", sector);
                        sector_written = write_sector_blocks(
                            app, sector, first_block, blocks_in_sector, &auth_key, auth_key_type);
                        break;
                    }
                }
            }

            // Try Key B from dump
            if(!sector_written &&
               mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeB)) {
                memcpy(auth_key.data, sec_tr->key_b.data, sizeof(auth_key.data));
                auth_key_type = MfClassicKeyTypeB;

                for(int retry = 0; retry < 2 && !sector_written; retry++) {
                    if(retry > 0) {
                        FURI_LOG_D(TAG, "Retry %d with dump Key B", retry);
                        furi_delay_ms(200);
                    }

                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, auth_key_type, &auth_context);

                    if(error == MfClassicErrorNone) {
                        FURI_LOG_I(TAG, "Sector %zu: Auth with dump Key B", sector);
                        sector_written = write_sector_blocks(
                            app, sector, first_block, blocks_in_sector, &auth_key, auth_key_type);
                        break;
                    }
                }
            }
        }

        // OPTION 3: Last resort - try 0xFF even if not detected as magic
        if(!sector_written && !has_magic_keys) {
            FURI_LOG_W(TAG, "Sector %zu: Last resort - trying 0xFF keys", sector);

            memset(auth_key.data, 0xFF, sizeof(auth_key.data));
            auth_key_type = MfClassicKeyTypeA;

            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, &auth_key, auth_key_type, &auth_context);

            if(error == MfClassicErrorNone) {
                FURI_LOG_I(TAG, "Sector %zu: Auth with 0xFF (last resort)", sector);
                sector_written = write_sector_blocks(
                    app, sector, first_block, blocks_in_sector, &auth_key, auth_key_type);
            }
        }

        // Check if sector was written successfully
        if(!sector_written) {
            FURI_LOG_E(TAG, "Sector %zu: ALL authentication methods FAILED", sector);
            if(app->logger) {
                miband_logger_log(
                    app->logger,
                    LogLevelError,
                    "Failed to write sector %zu - all auth methods failed",
                    sector);
            }

            // Special debug for sector 1 (blocks 4-7)
            if(sector == 1) {
                FURI_LOG_E(TAG, "CRITICAL: Sector 1 (blocks 4-7) failed to write!");
                FURI_LOG_E(TAG, "This is likely your problem area.");
                FURI_LOG_E(TAG, "has_magic_keys: %d", has_magic_keys);

                // Log what keys we have in dump for sector 1
                if(mf_classic_is_key_found(app->mf_classic_data, 1, MfClassicKeyTypeA)) {
                    FURI_LOG_E(TAG, "Dump has Key A for sector 1");
                }
                if(mf_classic_is_key_found(app->mf_classic_data, 1, MfClassicKeyTypeB)) {
                    FURI_LOG_E(TAG, "Dump has Key B for sector 1");
                }
            }

            write_success = false;
            should_break = true;
            break;
        }
    }

    // Cleanup
    furi_string_free(progress_msg);

    // Final result
    if(write_success) {
        FURI_LOG_I(TAG, "Write operation COMPLETED SUCCESSFULLY");
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelInfo, "Write operation completed successfully");
        }
    } else {
        FURI_LOG_E(TAG, "Write operation FAILED");
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Write operation failed");
        }
    }

    return write_success;
}

/**
 * @brief NFC scanner callback for card detection
 *
 * Called from NFC worker thread when a card is detected.
 * Sends custom event to main thread if MfClassic card found.
 */
static void writer_scanner_callback(NfcScannerEvent event, void* context) {
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
 * @brief Execute write operation and show result
 *
 * Called after card is detected via async scanner.
 * Performs the sync write operation and displays result.
 */
static void writer_execute_and_show_result(MiBandNfcApp* app) {
    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Card detected, starting write");
    }

    popup_reset(app->popup);
    popup_set_header(app->popup, "Writing Data", 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Starting write...", 64, 18, AlignCenter, AlignTop);
    furi_delay_ms(500);

    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_blink_start_magenta);

    FURI_LOG_I(TAG, "Starting write operation");
    bool write_result = miband_write_with_sync_approach(app);

    popup_reset(app->popup);

    if(write_result) {
        if(app->logger) {
            miband_logger_log(
                app->logger,
                LogLevelInfo,
                "Write successful for: %s",
                furi_string_get_cstr(app->file_path));
        }
        notification_message(app->notifications, &sequence_success);
        popup_set_header(app->popup, "Write Success!", 2, 2, AlignLeft, AlignTop);
        popup_set_text(app->popup, "Data written\nsuccessfully", 2, 16, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 68, 6, &I_DolphinSuccess_91x55);
        furi_delay_ms(2000);

        if(app->verify_after_write) {
            popup_set_text(app->popup, "Auto-verifying...", 64, 20, AlignCenter, AlignTop);
            furi_delay_ms(500);
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneVerify);
        } else {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MiBandNfcSceneMainMenu);
        }
    } else {
        if(app->logger) {
            miband_logger_log(
                app->logger,
                LogLevelError,
                "Write failed for: %s",
                furi_string_get_cstr(app->file_path));
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Write Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(
            app->popup,
            "Could not write\ndata to Mi Band\n\nCheck position",
            64,
            20,
            AlignCenter,
            AlignTop);
        FURI_LOG_E(TAG, "Write failed");

        furi_delay_ms(3000);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
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
void miband_nfc_scene_writer_on_enter(void* context) {
    furi_assert(context);
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
            "Write operation started for: %s",
            furi_string_get_cstr(app->file_path));
    }
    popup_reset(app->popup);

    if(app->current_operation == OperationTypeWriteOriginal) {
        popup_set_header(app->popup, "Write Original Data", 64, 4, AlignCenter, AlignTop);
    } else {
        popup_set_header(app->popup, "Write Data", 64, 4, AlignCenter, AlignTop);
    }

    popup_set_text(
        app->popup,
        "Place Mi Band near\nthe Flipper Zero\n\nWaiting for card...",
        64,
        20,
        AlignCenter,
        AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdWriter);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    FURI_LOG_I(TAG, "Waiting for Mi Band to be placed...");

    // Start async NFC scanner - event loop stays free, Back button works
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, writer_scanner_callback, app);
    app->is_scan_active = true;
}

/**
 * @brief Scene event handler
 *
 * Handles card detection (from async scanner) and Back button.
 *
 * @param context Pointer to MiBandNfcApp instance
 * @param event Scene manager event
 * @return true if event was consumed, false otherwise
 */
bool miband_nfc_scene_writer_on_event(void* context, SceneManagerEvent event) {
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

            // Perform sync write operation
            writer_execute_and_show_result(app);
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
 *
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_writer_on_exit(void* context) {
    furi_assert(context);
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
