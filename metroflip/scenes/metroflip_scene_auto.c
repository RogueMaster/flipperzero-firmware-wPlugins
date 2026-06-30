#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include <furi.h>
#include <bit_lib.h>
#include <lib/nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "keys.h"
#include "desfire.h"
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include "../api/metroflip/metroflip_api.h"
#define TAG "Metroflip:Scene:Auto"

static NfcCommand
    metroflip_scene_detect_desfire_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfDesfire);

    Metroflip* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfDesfirePollerEvent* mf_desfire_event = event.event_data;
    if(mf_desfire_event->type == MfDesfirePollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfDesfire, nfc_poller_get_data(app->poller));
        const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
        furi_string_reset(app->text_box_store);
        app->card_type = desfire_type(data);
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);

        command = NfcCommandStop;
    } else if(mf_desfire_event->type == MfDesfirePollerEventTypeReadFailed) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandContinue;
    }

    return command;
}

void metroflip_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    Metroflip* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "test");

        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        if(event.data.protocols && *event.data.protocols == NfcProtocolMfClassic) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolMfDesfire) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolFelica) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolIso14443_4b) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolIso14443_4a) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolSt25tb) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolMfUltralight) {
            nfc_detected_protocols_set(
                app->detected_protocols, event.data.protocols, event.data.protocol_num);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        }else {
            const NfcProtocol* invalid_protocol = (const NfcProtocol*)NfcProtocolInvalid;
            nfc_detected_protocols_set(app->detected_protocols, invalid_protocol, 0);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerDetect);
        }
    }
}

void metroflip_scene_auto_on_enter(void* context) {
    Metroflip* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, metroflip_scene_detect_scan_callback, app);

    metroflip_app_blink_start(app);
}

bool metroflip_scene_auto_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    UNUSED(app);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerDetect) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->auto_mode = true;
            FURI_LOG_I(
                TAG, "proto: %d", nfc_detected_protocols_get_protocol(app->detected_protocols, 0));
            if(nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
               NfcProtocolMfClassic) {
                MfClassicData* mfc_data = mf_classic_alloc();
                app->data_loaded = false;
                CardType card_type = determine_card_type(app->nfc, mfc_data, app->data_loaded);
                mf_classic_free(mfc_data);
                app->mfc_card_type = card_type;
                Popup* popup = app->popup;
                UNUSED(popup);
                switch(card_type) {
                case CARD_TYPE_METROMONEY:
                    app->card_type = "metromoney";
                    FURI_LOG_I(TAG, "Detected: Metromoney\n");
                    break;
                case CARD_TYPE_CHARLIECARD:
                    app->card_type = "charliecard";
                    FURI_LOG_I(TAG, "Detected: CharlieCard\n");
                    break;
                case CARD_TYPE_SMARTRIDER:
                    app->card_type = "smartrider";
                    FURI_LOG_I(TAG, "Detected: SmartRider\n");
                    break;
                case CARD_TYPE_TROIKA:
                    app->card_type = "troika";
                    FURI_LOG_I(TAG, "Detected: Troika\n");
                    break;
                case CARD_TYPE_RENFE_SUM10:
                    app->card_type = "renfe_sum10";
                    FURI_LOG_I(TAG, "Detected: RENFE Suma 10\n");
                    break;
                case CARD_TYPE_RENFE_REGULAR:
                    app->card_type = "renfe_regular";
                    FURI_LOG_I(TAG, "Detected: RENFE Regular\n");
                    break;
                case CARD_TYPE_UNKNOWN:
                    app->card_type = "Unknown Card";
                    popup_set_header(popup, "Unsupported\n card", 58, 31, AlignLeft, AlignTop);
                    break;
                default:
                    app->card_type = "Unknown Card";
                    FURI_LOG_I(TAG, "Detected: Unknown card type\n");
                    popup_set_header(popup, "Unsupported\n card", 58, 31, AlignLeft, AlignTop);
                    break;
                }
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolIso14443_4b) {
                app->card_type = "calypso";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolFelica) {
                app->card_type = "suica";
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolMfDesfire) {
                app->is_desfire = true;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
                nfc_poller_start(app->poller, metroflip_scene_detect_desfire_poller_callback, app);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolIso14443_4a) {
                app->card_type = "atr"; // place holder for now
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolSt25tb) {
                FURI_LOG_I(TAG, "Protocol is ST25TB");
                app->card_type = "intertic"; // place holder for now
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolMfUltralight) {
                FURI_LOG_I(TAG, "Protocol is MfUl");
                app->card_type = "trt"; // place holder for now
                app->is_desfire = false;
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            }else if(
                nfc_detected_protocols_get_protocol(app->detected_protocols, 0) ==
                NfcProtocolInvalid) {
                app->card_type = "Unknown Card";
                Popup* popup = app->popup;
                popup_set_header(popup, "Card\n Unsupported", 58, 31, AlignLeft, AlignTop);
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            } else {
                Popup* popup = app->popup;
                app->card_type = "Unknown Card";
                popup_set_header(popup, "Card\n Unsupported", 68, 30, AlignLeft, AlignTop);
                scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

void metroflip_scene_auto_on_exit(void* context) {
    Metroflip* app = context;
    if(!app->auto_mode) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
    }
    app->auto_mode = false;
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}
