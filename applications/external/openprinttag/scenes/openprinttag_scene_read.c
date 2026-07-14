#include "../openprinttag_i.h"

static void openprinttag_scanner_callback(NfcScannerEvent event, void* context) {
    OpenPrintTag* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        // Check if ISO15693 was detected
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            if(event.data.protocols[i] == NfcProtocolIso15693_3) {
                app->detected_protocol = NfcProtocolIso15693_3;
                view_dispatcher_send_custom_event(app->view_dispatcher, 1);
                return;
            }
        }
    }
}

static NfcCommand openprinttag_poller_callback(NfcGenericEvent event, void* context) {
    OpenPrintTag* app = context;

    // Check if reading is complete (protocol will be set)
    if(event.protocol == NfcProtocolIso15693_3) {
        // Data has been read, notify the scene
        view_dispatcher_send_custom_event(app->view_dispatcher, 2);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

void openprinttag_scene_read_on_enter(void* context) {
    OpenPrintTag* app = context;

    // Show loading view
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewLoading);

    // Allocate NFC device if not already allocated
    if(!app->nfc_device) {
        app->nfc_device = nfc_device_alloc();
    }

    // Start scanner
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->nfc_scanner, openprinttag_scanner_callback, app);
}

bool openprinttag_scene_read_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            // Tag detected, stop scanner and start poller
            nfc_scanner_stop(app->nfc_scanner);
            nfc_scanner_free(app->nfc_scanner);
            app->nfc_scanner = NULL;

            app->nfc_poller = nfc_poller_alloc(app->nfc, app->detected_protocol);
            nfc_poller_start(app->nfc_poller, openprinttag_poller_callback, app);
            consumed = true;
        } else if(event.event == 2) {
            // Reading complete
            nfc_poller_stop(app->nfc_poller);

            // Get the ISO15693 data
            const Iso15693_3Data* iso_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolIso15693_3);

            if(iso_data) {
                // Read all blocks and look for NDEF data
                uint16_t block_count = iso15693_3_get_block_count(iso_data);
                uint8_t block_size = iso15693_3_get_block_size(iso_data);

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

    if(app->nfc_scanner) {
        nfc_scanner_stop(app->nfc_scanner);
        nfc_scanner_free(app->nfc_scanner);
        app->nfc_scanner = NULL;
    }

    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
}
