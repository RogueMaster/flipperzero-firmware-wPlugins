#include "../saflip.h"
#include "../util/saflok/formats/mfc.h"
#include "protocols/mf_classic/mf_classic_poller.h"
#include <dolphin/dolphin.h>

enum {
    SaflipSceneReadCardEventComplete,
    SaflipSceneReadCardEventCancel
};

void saflip_scene_write_card_popup_callback(void* context) {
    SaflipApp* app = context;

    // Close popup and return to card detection view
    view_dispatcher_send_custom_event(app->view_dispatcher, SaflipSceneReadCardEventCancel);
}

NfcCommand saflip_scene_write_card_poller_callback(NfcGenericEvent event, void* context) {
    SaflipApp* app = context;

    NfcCommand result = NfcCommandContinue;

    switch(event.protocol) {
    case NfcProtocolMfClassic:
        MfClassicPollerEvent* mfc = event.event_data;

        saflok_generate(app->nfc_device, app->uid, app->uid_len, app->data);
        saflok_generate_mf_classic_variable_keys(
            app->nfc_device, app->keys, app->variable_keys, app->variable_keys_optional_function);
        saflok_generate_mf_classic_logs(app->nfc_device, app->log, app->log_entries);
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);

        switch(mfc->type) {
        case MfClassicPollerEventTypeRequestMode:
            mfc->data->poller_mode.mode = MfClassicPollerModeWrite;

            // Use target card's UID
            const MfClassicData* tag_data = nfc_poller_get_data(app->nfc_poller);
            app->uid_len = tag_data->iso14443_3a_data->uid_len;
            memcpy(app->uid, tag_data->iso14443_3a_data->uid, app->uid_len);
            break;

        case MfClassicPollerEventTypeRequestSectorTrailer:
            // TODO: Can't write to sector if key isn't already correct.
            uint8_t sector = mfc->data->sec_tr_data.sector_num;
            uint8_t sec_tr = mf_classic_get_sector_trailer_num_by_sector(sector);
            if(mf_classic_is_block_read(mfc_data, sec_tr)) {
                mfc->data->sec_tr_data.sector_trailer = mfc_data->block[sec_tr];
                mfc->data->sec_tr_data.sector_trailer_provided = true;
            } else {
                mfc->data->sec_tr_data.sector_trailer_provided = false;
            }
            break;

        case MfClassicPollerEventTypeRequestWriteBlock:
            uint8_t block_num = mfc->data->write_block_data.block_num;
            if(mf_classic_is_block_read(mfc_data, block_num)) {
                mfc->data->write_block_data.write_block = mfc_data->block[block_num];
                mfc->data->write_block_data.write_block_provided = true;
            } else {
                mfc->data->write_block_data.write_block_provided = false;
            }
            break;

        case MfClassicPollerEventTypeSuccess:
            result = NfcCommandStop;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SaflipSceneReadCardEventComplete);

            break;

        case MfClassicPollerEventTypeFail:
        case MfClassicPollerEventTypeCardLost:
            popup_reset(app->popup);
            popup_set_header(app->popup, "Lost card!", 64, 2, AlignCenter, AlignTop);
            popup_set_icon(app->popup, 21, 13, &I_dolph_cry_49x54);
            popup_set_timeout(app->popup, 1000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, saflip_scene_write_card_popup_callback);
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

void saflip_scene_write_card_on_enter(void* context) {
    SaflipApp* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    popup_reset(app->popup);
    popup_set_header(app->popup, "Don't move", 85, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 12, 23, &A_Loading_24);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);

    NfcProtocol protocol = scene_manager_get_scene_state(app->scene_manager, SaflipSceneReadCard);
    app->nfc_poller = nfc_poller_alloc(app->nfc, protocol);
    nfc_device_clear(app->nfc_device);
    nfc_poller_start(app->nfc_poller, saflip_scene_write_card_poller_callback, app);

    notification_message(app->notifications, &sequence_blink_start_green);
}

bool saflip_scene_write_card_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SaflipSceneReadCardEventComplete) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SaflipSceneOptions);
        } else if(event.event == SaflipSceneReadCardEventCancel) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SaflipSceneDetectCard);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // We don't want to go back to the Read Card scene
        uint32_t scenes[] = {
            SaflipSceneOptions,
            SaflipSceneDetectCard,
            SaflipSceneFileSelect,
            SaflipSceneStart,
        };

        consumed = scene_manager_search_and_switch_to_previous_scene_one_of(
            app->scene_manager, scenes, COUNT_OF(scenes));
    }

    return consumed;
}

void saflip_scene_write_card_on_exit(void* context) {
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
