#include "../saflip.h"
#include "../util/saflok/formats/mfc.h"
#include "protocols/mf_classic/mf_classic_poller.h"
#include <dolphin/dolphin.h>

enum {
    SaflipSceneReadCardEventComplete,
    SaflipSceneReadCardEventCancel
};

void saflip_scene_read_card_popup_callback(void* context) {
    SaflipApp* app = context;

    // Close popup and return to card detection view
    view_dispatcher_send_custom_event(app->view_dispatcher, SaflipSceneReadCardEventCancel);
}

NfcCommand saflip_scene_read_card_poller_callback(NfcGenericEvent event, void* context) {
    SaflipApp* app = context;

    NfcCommand result = NfcCommandContinue;

    switch(event.protocol) {
    case NfcProtocolMfClassic:
        MfClassicPollerEvent* mfc = event.event_data;

        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->nfc_poller));
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);

        switch(mfc->type) {
        case MfClassicPollerEventTypeRequestMode:
            mfc->data->poller_mode.mode = MfClassicPollerModeRead;
            break;

        case MfClassicPollerEventTypeRequestReadSector:
            // In case a card has a sector with abnormal keys, also try all-FF and all-00 keys.
            static enum {
                KeyTypeNormal,
                KeyTypeFF,
                KeyType00,
                KeyTypeExhausted
            } try_key_type = KeyTypeNormal;

            // Find next unread sector
            while(app->num_store < MF_CLASSIC_TOTAL_SECTORS_MAX) {
                uint8_t block = mf_classic_get_first_block_num_of_sector(app->num_store);
                if(mf_classic_is_block_read(mfc_data, block)) {
                    // Always start with Normal key type for each sector
                    try_key_type = KeyTypeNormal;
                    app->num_store++;
                } else {
                    break;
                }
            }

            // Check if we've read the whole card
            if(app->num_store >= MF_CLASSIC_TOTAL_SECTORS_MAX) {
                mfc->data->read_sector_request_data.key_provided = false;
                break;
            }

            // Populate request data
            mfc->data->read_sector_request_data.sector_num = app->num_store;
            mfc->data->read_sector_request_data.key_type = MfClassicKeyTypeA;
            mfc->data->read_sector_request_data.key_provided = true;

            switch(try_key_type++) {
            case KeyTypeNormal:
                size_t uid_len = 0;
                const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &uid_len);
                saflok_mfc_generate_key(
                    uid, app->num_store, mfc->data->read_sector_request_data.key.data);
                break;
            case KeyTypeFF:
                memset(mfc->data->read_sector_request_data.key.data, 0xFF, 6);
                break;
            case KeyType00:
                memset(mfc->data->read_sector_request_data.key.data, 0x00, 6);
                break;
            case KeyTypeExhausted:
                // We're unable to read this sector, reset key type and move onto the next one
                mfc->data->read_sector_request_data.key_provided = false;
                try_key_type = KeyTypeNormal;
                app->num_store++;
                break;
            }

            break;

        case MfClassicPollerEventTypeSuccess:
            result = NfcCommandStop;

            if(saflok_parse(app->nfc_device, app->uid, &app->uid_len, app->data)) {
                // Only read log entries and variable keys if successfully parsed
                app->log_entries = saflok_parse_mf_classic_logs(app->nfc_device, app->log);
                app->variable_keys = saflok_parse_mf_classic_variable_keys(
                    app->nfc_device, app->keys, &app->variable_keys_optional_function);
            }

            view_dispatcher_send_custom_event(
                app->view_dispatcher, SaflipSceneReadCardEventComplete);

            break;

        case MfClassicPollerEventTypeFail:
            popup_reset(app->popup);
            popup_set_header(app->popup, "Failed to read!", 64, 2, AlignCenter, AlignTop);
            popup_set_icon(app->popup, 21, 13, &I_dolph_cry_49x54);
            popup_set_timeout(app->popup, 1000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, saflip_scene_read_card_popup_callback);
            popup_set_context(app->popup, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
            break;

        case MfClassicPollerEventTypeCardLost:
            popup_reset(app->popup);
            popup_set_header(app->popup, "Lost card!", 64, 2, AlignCenter, AlignTop);
            popup_set_icon(app->popup, 21, 13, &I_dolph_cry_49x54);
            popup_set_timeout(app->popup, 1000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, saflip_scene_read_card_popup_callback);
            popup_set_context(app->popup, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
            break;

        default:
            // Ignore other events
            break;
        }
        break;
    default:
        FURI_LOG_W(TAG, "Poller event from unknown protocol type %d!", event.protocol);
        break;
    }

    return result;
}

void saflip_scene_read_card_on_enter(void* context) {
    SaflipApp* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    popup_reset(app->popup);
    popup_set_header(app->popup, "Don't move", 85, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 12, 23, &A_Loading_24);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);

    NfcProtocol protocol = scene_manager_get_scene_state(app->scene_manager, SaflipSceneReadCard);
    app->nfc_poller = nfc_poller_alloc(app->nfc, protocol);
    app->num_store = 0;
    nfc_device_clear(app->nfc_device);
    nfc_poller_start(app->nfc_poller, saflip_scene_read_card_poller_callback, app);

    notification_message(app->notifications, &sequence_blink_start_green);
}

bool saflip_scene_read_card_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SaflipSceneReadCardEventComplete) {
            if(saflok_parse(app->nfc_device, app->uid, &app->uid_len, app->data)) {
                scene_manager_next_scene(app->scene_manager, SaflipSceneInfo);
            } else {
                popup_reset(app->popup);
                popup_set_header(app->popup, "Invalid card!", 64, 2, AlignCenter, AlignTop);
                popup_set_text(app->popup, "Failed to\nparse data.", 78, 16, AlignLeft, AlignTop);
                popup_set_icon(app->popup, 21, 13, &I_dolph_cry_49x54);
                popup_set_timeout(app->popup, 1500);
                popup_enable_timeout(app->popup);
                popup_set_callback(app->popup, saflip_scene_read_card_popup_callback);
                popup_set_context(app->popup, app);
                view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);
            }
        } else if(event.event == SaflipSceneReadCardEventCancel) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SaflipSceneDetectCard);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Skip past the detect card scene
        // If more scenes are added that can trigger the detect card scene, they should be added here as well
        consumed = scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SaflipSceneStart);
    }

    return consumed;
}

void saflip_scene_read_card_on_exit(void* context) {
    SaflipApp* app = context;

    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);

    if(app->nfc_keys_dictionary != NULL) {
        keys_dict_free(app->nfc_keys_dictionary);
        app->nfc_keys_dictionary = NULL;
    }

    // Clear view
    popup_reset(app->popup);
    notification_message(app->notifications, &sequence_blink_stop);
}
