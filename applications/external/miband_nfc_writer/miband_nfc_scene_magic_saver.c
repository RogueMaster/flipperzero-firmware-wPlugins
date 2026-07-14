/**
 * @file miband_nfc_scene_magic_saver.c
 * @brief Magic dump converter with GUI updates
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

static bool miband_nfc_save_magic_dump(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        FURI_LOG_E(TAG, "No valid NFC data to save");
        return false;
    }

    FURI_LOG_D(TAG, "Preparing magic dump with 0xFF keys");

    MfClassicData* magic_data = mf_classic_alloc();
    if(!magic_data) {
        FURI_LOG_E(TAG, "Failed to allocate MfClassic data");
        return false;
    }

    mf_classic_copy(magic_data, app->mf_classic_data);
    size_t total_sectors = mf_classic_get_total_sectors_num(magic_data->type);
    // FIX: Use app->temp_text_buffer instead of local FuriString.
    // popup_set_text stores pointer only - local string buffer could be
    // invalidated between renders.
    for(size_t sector_idx = 0; sector_idx < total_sectors; sector_idx++) {
        furi_string_printf(
            app->temp_text_buffer,
            "Converting keys\nSector %zu/%zu\n\n",
            sector_idx + 1,
            total_sectors);

        uint32_t percent = ((sector_idx + 1) * 100) / total_sectors;
        furi_string_cat_str(app->temp_text_buffer, "[");
        for(uint8_t i = 0; i < 20; i++) {
            if(i < (percent / 5)) {
                furi_string_cat_str(app->temp_text_buffer, "=");
            } else {
                furi_string_cat_str(app->temp_text_buffer, " ");
            }
        }
        furi_string_cat_printf(app->temp_text_buffer, "]\n%lu%%", percent);

        popup_set_text(
            app->popup, furi_string_get_cstr(app->temp_text_buffer), 64, 22, AlignCenter, AlignTop);

        furi_delay_ms(20);

        uint8_t sector_trailer_block_idx;
        if(sector_idx < 32) {
            sector_trailer_block_idx = sector_idx * 4 + 3;
        } else {
            sector_trailer_block_idx = 32 * 4 + (sector_idx - 32) * 16 + 15;
        }

        MfClassicBlock* sector_trailer = &magic_data->block[sector_trailer_block_idx];

        memset(&sector_trailer->data[0], 0xFF, 6);
        sector_trailer->data[6] = 0xFF;
        sector_trailer->data[7] = 0x07;
        sector_trailer->data[8] = 0x80;
        sector_trailer->data[9] = 0x69;
        memset(&sector_trailer->data[10], 0xFF, 6);

        FURI_BIT_SET(magic_data->key_a_mask, sector_idx);
        FURI_BIT_SET(magic_data->key_b_mask, sector_idx);
    }

    popup_set_text(app->popup, "Saving file...", 64, 30, AlignCenter, AlignTop);
    furi_delay_ms(100);

    FuriString* output_path = furi_string_alloc();
    furi_string_set(output_path, app->file_path);

    size_t ext_pos = furi_string_search_rchar(output_path, '.');
    if(ext_pos != FURI_STRING_FAILURE) {
        furi_string_left(output_path, ext_pos);
    }
    furi_string_cat_str(output_path, "_magic.nfc");

    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, magic_data);
    bool save_success = nfc_device_save(app->nfc_device, furi_string_get_cstr(output_path));
    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->mf_classic_data);

    FURI_LOG_I(
        TAG,
        "Magic dump save %s: %s",
        save_success ? "success" : "failed",
        furi_string_get_cstr(output_path));

    mf_classic_free(magic_data);
    furi_string_free(output_path);

    return save_success;
}

void miband_nfc_scene_magic_saver_on_enter(void* context) {
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
            "Magic dump conversion started for: %s",
            furi_string_get_cstr(app->file_path));
    }
    popup_reset(app->popup);
    popup_set_header(app->popup, "Saving Magic Dump", 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Preparing...", 64, 22, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMagicEmulator);
    notification_message(app->notifications, &sequence_blink_start_magenta);

    bool save_success = miband_nfc_save_magic_dump(app);

    popup_reset(app->popup);

    if(save_success) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelInfo, "Magic dump saved with _magic suffix");
        }
        notification_message(app->notifications, &sequence_success);
        popup_set_header(app->popup, "Success!", 2, 2, AlignLeft, AlignTop);
        popup_set_text(app->popup, "Magic dump\nsaved OK!", 2, 16, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 68, 6, &I_DolphinSuccess_91x55);
        furi_delay_ms(2000);
    } else {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to save magic dump");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Could not save file", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
    }

    scene_manager_search_and_switch_to_another_scene(app->scene_manager, MiBandNfcSceneMainMenu);
}

bool miband_nfc_scene_magic_saver_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void miband_nfc_scene_magic_saver_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
