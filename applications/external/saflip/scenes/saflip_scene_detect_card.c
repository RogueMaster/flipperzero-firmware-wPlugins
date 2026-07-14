#include "../saflip.h"
#include <dolphin/dolphin.h>

enum {
    SaflipSceneDetectCardEventRestartScanner,
    SaflipSceneDetectCardEventFoundCard
};

void saflip_scene_detect_card_scanner_callback(NfcScannerEvent event, void* context) {
    SaflipApp* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        NfcProtocol protocol = NfcProtocolInvalid;

        for(size_t i = 0; i < event.data.protocol_num; i++) {
            switch(event.data.protocols[i]) {
            case NfcProtocolMfClassic:
                protocol = event.data.protocols[i];
                break;
            default:
                break;
            }
        }

        if(protocol == NfcProtocolInvalid) {
            // Unsupported card, send event to restart scanner
            FURI_LOG_W(TAG, "Unsupported card detected");
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SaflipSceneDetectCardEventRestartScanner);
        } else {
            // Found a supported card type, try to read or write
            scene_manager_set_scene_state(app->scene_manager, SaflipSceneReadCard, protocol);
            scene_manager_set_scene_state(app->scene_manager, SaflipSceneWriteCard, protocol);
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SaflipSceneDetectCardEventFoundCard);
        }
    }
}

void saflip_scene_detect_card_on_enter(void* context) {
    SaflipApp* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    SaflipDetectCardMode mode =
        scene_manager_get_scene_state(app->scene_manager, SaflipSceneDetectCard);

    // Setup view
    popup_reset(app->popup);
    switch(mode) {
    case SaflipDetectCardModeRead:
        popup_set_header(app->popup, "Reading", 97, 15, AlignCenter, AlignTop);
        break;
    case SaflipDetectCardModeWrite:
        popup_set_header(app->popup, "Writing", 97, 15, AlignCenter, AlignTop);
        break;
    }
    popup_set_text(app->popup, "Hold card next\nto Flipper's back", 94, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 8, &I_NFC_manual_60x50);
    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewPopup);

    nfc_scanner_start(app->nfc_scanner, saflip_scene_detect_card_scanner_callback, app);

    notification_message(app->notifications, &sequence_blink_start_cyan);
}

bool saflip_scene_detect_card_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SaflipSceneDetectCardEventRestartScanner) {
            nfc_scanner_stop(app->nfc_scanner);
            nfc_scanner_start(app->nfc_scanner, saflip_scene_detect_card_scanner_callback, app);
            consumed = true;
        } else if(event.event == SaflipSceneDetectCardEventFoundCard) {
            SaflipDetectCardMode mode =
                scene_manager_get_scene_state(app->scene_manager, SaflipSceneDetectCard);

            switch(mode) {
            case SaflipDetectCardModeRead:
                scene_manager_next_scene(app->scene_manager, SaflipSceneReadCard);
                break;
            case SaflipDetectCardModeWrite:
                scene_manager_next_scene(app->scene_manager, SaflipSceneWriteCard);
                break;
            }

            consumed = true;
        }
    }

    return consumed;
}

void saflip_scene_detect_card_on_exit(void* context) {
    SaflipApp* app = context;

    nfc_scanner_stop(app->nfc_scanner);

    // Clear view
    popup_reset(app->popup);
    notification_message(app->notifications, &sequence_blink_stop);
}
