#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

void miband_nfc_scene_scanner_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    if(event.type == NfcScannerEventTypeDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MiBandNfcCustomEventCardDetected);

        if(*event.data.protocols == NfcProtocolMfClassic) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiBandNfcCustomEventMfClassicCard);
        } else {
            view_dispatcher_send_custom_event(app->view_dispatcher, MiBandNfcCustomEventWrongCard);
        }
    }
}

void miband_nfc_scene_scanner_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Scanning for Mi Band", 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "Place Mi Band near\nthe Flipper Zero\n\nSearching...",
        64,
        22,
        AlignCenter,
        AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);

    app->poller = NULL;
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, miband_nfc_scene_scanner_scan_callback, app);
    app->is_scan_active = true;

    notification_message(app->notifications, &sequence_blink_start_cyan);
    FURI_LOG_I(TAG, "Enter scanner scene");
}

bool miband_nfc_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected:
            popup_reset(app->popup);
            popup_set_header(app->popup, "Mi Band Found!", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Hold steady...\nDon't move the card\n\nAnalyzing...",
                64,
                22,
                AlignCenter,
                AlignTop);
            consumed = true;
            break;

        case MiBandNfcCustomEventCardLost:
            popup_reset(app->popup);
            popup_set_header(app->popup, "Card Lost", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Mi Band moved away\nPlace it back near\nthe Flipper",
                64,
                22,
                AlignCenter,
                AlignTop);
            consumed = true;
            break;

        case MiBandNfcCustomEventWrongCard:
            app->is_scan_active = false;
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }

            popup_reset(app->popup);
            popup_set_header(app->popup, "Wrong Card Type", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "This is not a\nMifare Classic card\n\nTry again",
                64,
                22,
                AlignCenter,
                AlignTop);

            notification_message(app->notifications, &sequence_error);
            furi_delay_ms(2000);
            scene_manager_set_scene_state(app->scene_manager, MiBandNfcSceneScanner, 0);
            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneScanner);
            consumed = true;
            break;

        case MiBandNfcCustomEventMfClassicCard:
            app->is_scan_active = false;
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }

            scene_manager_next_scene(app->scene_manager, MiBandNfcSceneWriter);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void miband_nfc_scene_scanner_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    app->is_scan_active = false;
    app->poller = NULL;

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
