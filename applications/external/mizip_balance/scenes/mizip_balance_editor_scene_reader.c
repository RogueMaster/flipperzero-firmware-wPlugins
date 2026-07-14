#include "../mizip_balance_editor_i.h"

static NfcCommand
    mizip_balance_editor_scene_reader_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfClassic);
    MiZipBalanceEditorApp* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfClassicPollerEvent* mfc_event = event.event_data;
    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        FURI_LOG_D("MiZipBalanceEditor", "Card detected");
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardDetected);
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        FURI_LOG_D("MiZipBalanceEditor", "Card lost");
        command = NfcCommandStop;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller request mode");
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        if(app->current_sector >= MIZIP_SECTOR_COUNT) {
            mfc_event->data->read_sector_request_data.key_provided = false;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiZipBalanceEditorCustomEventPollerDone);
        } else {
            // ALWAYS use Key B for ALL MiZIP sectors (as specified)
            MfClassicKey key_b;
            memcpy(key_b.data, app->calculated_keyB[app->current_sector], MIZIP_KEY_LENGTH);
            mfc_event->data->read_sector_request_data.sector_num = app->current_sector;
            mfc_event->data->read_sector_request_data.key = key_b;
            mfc_event->data->read_sector_request_data.key_type = MfClassicKeyTypeB;
            mfc_event->data->read_sector_request_data.key_provided = true;
            app->current_sector++;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller success");
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        mf_classic_copy(
            app->mf_classic_data, nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic));
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller failed");
        command = NfcCommandStop;
    }
    return command;
}

void mizip_balance_editor_scene_reader_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    popup_set_icon(app->popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    app->current_sector = 0;

    // Set the keys for sector 0 (MiZIP standard)
    const uint8_t keyA[] = MIZIP_KEYA_0_BYTES;
    memcpy(app->calculated_keyA[0], keyA, MIZIP_KEY_LENGTH);
    // Key B in sector 0 is standard for MiZIP.
    const uint8_t keyB_0[] = MIZIP_KEYB_0_BYTES;
    memcpy(app->calculated_keyB[0], keyB_0, MIZIP_KEY_LENGTH);

    // Generate keys for sectors 1-4 based on the UID
    mizip_generate_key(app->uid, app->calculated_keyA, app->calculated_keyB);

    // Log of keys generated for debugging (focus on sector 2 for credit data)
    for(int i = 0; i < 5; i++) {
        char key_b_str[50];
        snprintf(
            key_b_str,
            sizeof(key_b_str),
            "Sector %d Key B: %02X %02X %02X %02X %02X %02X",
            i,
            app->calculated_keyB[i][0],
            app->calculated_keyB[i][1],
            app->calculated_keyB[i][2],
            app->calculated_keyB[i][3],
            app->calculated_keyB[i][4],
            app->calculated_keyB[i][5]);
        FURI_LOG_I("MiZipBalanceEditor", key_b_str);
    }

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
    nfc_poller_start(app->poller, mizip_balance_editor_scene_reader_poller_callback, context);
    notification_message(app->notifications, &sequence_blink_start_cyan);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdReader);
}

bool mizip_balance_editor_scene_reader_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventCardDetected) {
            popup_set_header(app->popup, "Reading...", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventCardLost) {
            popup_set_header(app->popup, "Card lost...", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventWrongCard) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->is_scan_active = false;
            popup_set_header(app->popup, "Wrong tag", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventPollerDone) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            if(mf_classic_is_card_read(app->mf_classic_data)) {
                dolphin_deed(DolphinDeedNfcReadSuccess);
                FURI_LOG_D("MiZipBalanceEditor", "Card readed");

                // Assume data is MiZip at this point
                app->is_valid_mizip_data = true;
                FURI_LOG_I("MiZipBalanceEditor", "Assume MiZIP data is valid");
            } else {
                FURI_LOG_D("MiZipBalanceEditor", "Card not readed");
                app->is_valid_mizip_data = false;
            }

            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowBalance);
            consumed = true;
        }
    }
    return consumed;
}

void mizip_balance_editor_scene_reader_on_exit(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
