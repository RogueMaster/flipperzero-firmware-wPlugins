#include "../openprinttag_i.h"

static void openprinttag_scanner_callback(NfcScannerEvent event, void* context) {
    OpenPrintTag* app = context;

    FURI_LOG_I(TAG, "Scanner callback: event type = %d", event.type);

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "Tag detected! Protocols found: %zu", event.data.protocol_num);

        // Check if ISO15693 was detected
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            FURI_LOG_I(TAG, "Protocol %zu: %d", i, event.data.protocols[i]);
            // SLIX/SLIX2 chips are reported as their own protocol, a child of ISO15693-3.
            // The ISO15693-3 poller reads them just the same.
            if(event.data.protocols[i] == NfcProtocolIso15693_3 ||
               event.data.protocols[i] == NfcProtocolSlix) {
                FURI_LOG_I(TAG, "ISO15693 detected! Starting poller...");
                app->detected_protocol = NfcProtocolIso15693_3;
                view_dispatcher_send_custom_event(app->view_dispatcher, 1);
                return;
            }
        }
        FURI_LOG_W(TAG, "Tag detected but not ISO15693");
    }
}

static NfcCommand openprinttag_poller_callback(NfcGenericEvent event, void* context) {
    OpenPrintTag* app = context;

    FURI_LOG_I(TAG, "Poller callback: protocol = %d", event.protocol);

    // Check if reading is complete (protocol will be set)
    if(event.protocol == NfcProtocolIso15693_3) {
        FURI_LOG_I(TAG, "ISO15693 data ready, stopping poller");
        // Data has been read, notify the scene
        view_dispatcher_send_custom_event(app->view_dispatcher, 2);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

void openprinttag_scene_read_on_enter(void* context) {
    OpenPrintTag* app = context;

    FURI_LOG_I(TAG, "Read scene entered");
    app->read_retries = 0;

    // Show popup with scanning message
    // The popup is reset first so no timeout, callback or text from an earlier popup is left over
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "Scanning...", 64, 6, AlignCenter, AlignTop);
    popup_set_text(
        popup, "Place the tag\nnear Flipper\n\nBACK = cancel", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewPopup);

    // Allocate NFC device if not already allocated
    if(!app->nfc_device) {
        app->nfc_device = nfc_device_alloc();
        FURI_LOG_I(TAG, "NFC device allocated");
    }

    // Start scanner
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    FURI_LOG_I(TAG, "NFC scanner allocated, starting scan...");
    nfc_scanner_start(app->nfc_scanner, openprinttag_scanner_callback, app);
    FURI_LOG_I(TAG, "Waiting for ISO15693 tag...");
}

bool openprinttag_scene_read_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            // Tag detected, stop scanner and start poller
            FURI_LOG_I(TAG, "Event 1: Tag detected, switching to poller");
            nfc_scanner_stop(app->nfc_scanner);
            nfc_scanner_free(app->nfc_scanner);
            app->nfc_scanner = NULL;

            app->nfc_poller = nfc_poller_alloc(app->nfc, app->detected_protocol);
            nfc_poller_start(app->nfc_poller, openprinttag_poller_callback, app);
            consumed = true;
        } else if(event.event == 2) {
            // Reading complete
            FURI_LOG_I(TAG, "Event 2: Reading complete, parsing data");
            // The poller callback already returned NfcCommandStop. Do not call
            // nfc_poller_stop() here: it must run exactly once per start, and on_exit does it
            // (a second call fails a furi_check inside nfc_stop and crashes the app).

            // A tag that was reported without blocks is read again with a new poller
            const OpenPrintTagReadCheck check =
                openprinttag_check_read(app, openprinttag_poller_callback);
            if(check == OpenPrintTagReadRetried) return true;
            if(check == OpenPrintTagReadFailed) {
                scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneReadError);
                return true;
            }

            // Get the ISO15693 data from the POLLER (not the device!)
            const NfcDeviceData* poller_data = nfc_poller_get_data(app->nfc_poller);
            const Iso15693_3Data* iso_data = (const Iso15693_3Data*)poller_data;

            if(iso_data) {
                // Remember the UID for the result screen, the poller is freed when the scene exits
                memcpy(app->tag_uid, iso_data->uid, sizeof(app->tag_uid));
                app->has_tag_uid = true;

                // Read all blocks and look for NDEF data
                uint16_t block_count = iso15693_3_get_block_count(iso_data);
                uint8_t block_size = iso15693_3_get_block_size(iso_data);

                FURI_LOG_I(
                    TAG,
                    "Tag info: %d blocks x %d bytes = %d total",
                    block_count,
                    block_size,
                    block_count * block_size);

                // Build complete tag data
                size_t total_size = block_count * block_size;
                uint8_t* tag_memory = malloc(total_size);

                for(uint16_t i = 0; i < block_count; i++) {
                    const uint8_t* block = iso15693_3_get_block_data(iso_data, i);
                    if(block) {
                        memcpy(tag_memory + (i * block_size), block, block_size);
                    }
                }

                // Try to parse NDEF
                bool parse_success = openprinttag_parse_ndef(app, tag_memory, total_size);
                free(tag_memory);

                if(parse_success) {
                    scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneReadSuccess);
                } else {
                    scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneReadError);
                }
            } else {
                scene_manager_next_scene(app->scene_manager, OpenPrintTagSceneReadError);
            }
            consumed = true;
        }
    }

    return consumed;
}

void openprinttag_scene_read_on_exit(void* context) {
    OpenPrintTag* app = context;

    FURI_LOG_I(TAG, "Read scene exiting, cleaning up...");

    if(app->nfc_scanner) {
        nfc_scanner_stop(app->nfc_scanner);
        nfc_scanner_free(app->nfc_scanner);
        app->nfc_scanner = NULL;
        FURI_LOG_I(TAG, "Scanner stopped and freed");
    }

    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
        FURI_LOG_I(TAG, "Poller stopped and freed");
    }

    popup_reset(app->popup);
}
