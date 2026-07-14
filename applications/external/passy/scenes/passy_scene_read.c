#include "../passy_i.h"
#include "../passy_reader.h"
#include <dolphin/dolphin.h>

#define TAG "PassySceneRead"

static PassyReader* passy_reader = NULL;

void passy_scene_detect_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    Passy* passy = context;
    passy->proto = NULL;

    // detect the type of protocol, either 4a/4b and then run the specific poller callback functions yada yada yada
    if(event.type == NfcScannerEventTypeDetected) {
        if(event.data.protocols && *event.data.protocols == NfcProtocolIso14443_4a) {
            passy->proto = "4a";
        } else if(event.data.protocols && *event.data.protocols == NfcProtocolIso14443_4b) {
            passy->proto = "4b";
        } else {
            FURI_LOG_E(TAG, "Unknown protocol detected, %d", *event.data.protocols);
        }
        if(passy->proto && strlen(passy->proto) > 0) {
            view_dispatcher_send_custom_event(
                passy->view_dispatcher, PassyCustomEventReaderDetected);
        } else {
            view_dispatcher_send_custom_event(
                passy->view_dispatcher, PassyCustomEventReaderRestart);
        }
    }
}

void passy_scene_read_on_enter(void* context) {
    Passy* passy = context;
    dolphin_deed(DolphinDeedNfcRead);

    passy->poller = NULL;

    // Setup view
    Popup* popup = passy->popup;
    popup_set_header(popup, "Detecting...", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    passy->scanner = nfc_scanner_alloc(passy->nfc);
    nfc_scanner_start(
        passy->scanner, passy_scene_detect_scan_callback, passy); // starts a scanner instance

    passy_blink_start(passy);

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewPopup);
}

bool passy_scene_read_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;
    Popup* popup = passy->popup;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PassyCustomEventReaderSuccess) {
            if(passy->read_type == PassyReadCOM) {
                scene_manager_next_scene(passy->scene_manager, PassySceneAdvancedMenu);
            } else {
                scene_manager_next_scene(passy->scene_manager, PassySceneReadSuccess);
            }
            consumed = true;
        } else if(event.event == PassyCustomEventReaderError) {
            passy->last_sw = passy_reader->last_sw;
            scene_manager_next_scene(passy->scene_manager, PassySceneReadError);
            consumed = true;
        } else if(event.event == PassyCustomEventReaderRestart) {
            nfc_scanner_stop(passy->scanner);
            nfc_scanner_start(passy->scanner, passy_scene_detect_scan_callback, passy);
            consumed = true;
        } else if(event.event == PassyCustomEventReaderDetected) {
            nfc_scanner_stop(passy->scanner);
            nfc_scanner_free(passy->scanner);
            passy->scanner = NULL;
            if(strcmp(passy->proto, "4b") == 0) {
                FURI_LOG_I(TAG, "detected 4B protocol");
                passy->poller = nfc_poller_alloc(passy->nfc, NfcProtocolIso14443_4b);
                passy_reader = passy_reader_alloc(passy);
                nfc_poller_start(passy->poller, passy_reader_poller_callback, passy_reader);
                passy->bytes_total = 0;
            } else if(strcmp(passy->proto, "4a") == 0) {
                FURI_LOG_I(TAG, "detected 4A protocol");
                passy->poller = nfc_poller_alloc(passy->nfc, NfcProtocolIso14443_4a);
                passy_reader = passy_reader_alloc(passy);
                nfc_poller_start(passy->poller, passy_reader_poller_callback, passy_reader);
                passy->bytes_total = 0;
            } else {
                view_dispatcher_send_custom_event(
                    passy->view_dispatcher, PassyCustomEventReaderError);
            }
            popup_set_header(popup, "Detected", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == PassyCustomEventReaderAuthenticated) {
            popup_set_header(popup, "Authenticated", 68, 30, AlignLeft, AlignTop);
        } else if(event.event == PassyCustomEventReaderReading) {
            if(passy->bytes_total == 0) {
                popup_set_header(popup, "Reading", 68, 30, AlignLeft, AlignTop);
            } else {
                // Update the header with the current bytes read
                snprintf(
                    passy->text_store,
                    PASSY_TEXT_STORE_SIZE,
                    "Reading\n%d/%dk",
                    passy->offset,
                    (passy->bytes_total / 1024));
                popup_set_header(popup, passy->text_store, 68, 30, AlignLeft, AlignTop);
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_search_and_switch_to_previous_scene(
            passy->scene_manager, PassySceneMainMenu);
    }

    return consumed;
}

void passy_scene_read_on_exit(void* context) {
    Passy* passy = context;

    if(passy->poller) {
        nfc_poller_stop(passy->poller);
        nfc_poller_free(passy->poller);
        passy->poller = NULL;
    }
    if(passy_reader) {
        passy_reader_free(passy_reader);
        passy_reader = NULL;
    }
    if(passy->scanner) {
        nfc_scanner_stop(passy->scanner);
        nfc_scanner_free(passy->scanner);
        passy->scanner = NULL;
    }

    // Clear view
    popup_reset(passy->popup);

    passy_blink_stop(passy);
}
