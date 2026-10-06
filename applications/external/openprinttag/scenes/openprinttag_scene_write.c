#include "../openprinttag_i.h"

#include <nfc/helpers/iso13239_crc.h>
#include <toolbox/bit_buffer.h>

// Order of the rows on the edit screen, which is also the index passed to the click callback
typedef enum {
    WriteItemRemaining, // Information only
    WriteItemConsumedWeight,
    WriteItemAddConsumption,
    WriteItemSave,
} WriteItem;

typedef enum {
    WriteEventTagDetected = 1,
    WriteEventTagRead = 2,
    WriteEventSave = 3,
    WriteEventWriteDone = OpenPrintTagEventWriteDone,
    WriteEventWriteFailed = OpenPrintTagEventWriteFailed,
    WriteEventResultDismissed = 6, // Message shown, go back to the main menu
    WriteEventFailureDismissed = 7, // Message shown, go back to the edit screen
    WriteEventEditConsumed = 8, // OK on the total consumed row: open the number pad
    WriteEventConsumedEntered = 9, // A value was entered on the number pad
    WriteEventAddConsumption = 10, // OK on the add consumption row: open the number pad
} WriteEvent;

#define WRITE_MAX_BLOCK (32U)

// The single-block commands carry the block number in one byte
#define WRITE_MAX_BLOCK_COUNT (256U)

static void openprinttag_write_scanner_callback(NfcScannerEvent event, void* context) {
    OpenPrintTag* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        // Check if ISO15693 was detected
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            // SLIX/SLIX2 chips are reported as their own protocol, a child of ISO15693-3.
            // The ISO15693-3 poller reads and writes them just the same.
            if(event.data.protocols[i] == NfcProtocolIso15693_3 ||
               event.data.protocols[i] == NfcProtocolSlix) {
                app->detected_protocol = NfcProtocolIso15693_3;
                view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventTagDetected);
                return;
            }
        }
    }
}

static NfcCommand openprinttag_write_poller_callback(NfcGenericEvent event, void* context) {
    OpenPrintTag* app = context;

    if(event.protocol == NfcProtocolIso15693_3) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventTagRead);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void write_popup_to_menu_callback(void* context) {
    OpenPrintTag* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventResultDismissed);
}

static void write_popup_to_edit_callback(void* context) {
    OpenPrintTag* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventFailureDismissed);
}

// Shows a message for a moment, then goes back to the main menu, or to the edit screen (which
// still holds the entered value) if return_to_edit is set
static void write_show_result(
    OpenPrintTag* app,
    const char* header,
    const char* text,
    bool return_to_edit) {
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, header, 64, 10, AlignCenter, AlignTop);
    popup_set_text(popup, text, 64, 32, AlignCenter, AlignCenter);
    popup_set_context(popup, app);
    popup_set_callback(
        popup, return_to_edit ? write_popup_to_edit_callback : write_popup_to_menu_callback);
    popup_set_timeout(popup, 2500);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewPopup);
}

static void write_free_data(OpenPrintTag* app) {
    if(app->write_data) {
        free(app->write_data);
        app->write_data = NULL;
    }
    app->write_in_progress = false;
}

static void write_stop_poller(OpenPrintTag* app) {
    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
}

// Weight of a full spool, the most that can have been consumed
static uint32_t write_full_weight(const OpenPrintTag* app) {
    uint32_t total = app->tag_data.main.actual_netto_full_weight;
    if(total == 0) total = app->tag_data.main.nominal_netto_full_weight;
    return total;
}

// Makes the texts of the edit screen follow the entered consumed weight
static void write_update_items(OpenPrintTag* app) {
    char text[16];

    if(app->write_consumed_item) {
        snprintf(text, sizeof(text), "%lu g", app->temp_consumed_weight);
        variable_item_set_current_value_text(app->write_consumed_item, text);
    }

    if(app->write_remaining_item) {
        uint32_t total = write_full_weight(app);
        uint32_t remaining =
            total > app->temp_consumed_weight ? total - app->temp_consumed_weight : 0;
        snprintf(text, sizeof(text), "%lu g", remaining);
        variable_item_set_current_value_text(app->write_remaining_item, text);
    }
}

// The consumed weight row has three positions with the middle one always selected. Left and
// right change the exact value by one step and move the selection back to the middle, so both
// arrows stay available and values typed on the keyboard are not rounded.
#define WRITE_CONSUMED_STEP         (20U)
#define WRITE_CONSUMED_CENTER_INDEX (1U)

static uint32_t write_max_consumed(const OpenPrintTag* app) {
    uint32_t full_weight = write_full_weight(app);
    return full_weight > 0 ? full_weight : 100000;
}

static void consumed_weight_change_callback(VariableItem* item) {
    OpenPrintTag* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    if(index < WRITE_CONSUMED_CENTER_INDEX) {
        app->temp_consumed_weight = app->temp_consumed_weight > WRITE_CONSUMED_STEP ?
                                        app->temp_consumed_weight - WRITE_CONSUMED_STEP :
                                        0;
    } else if(index > WRITE_CONSUMED_CENTER_INDEX) {
        app->temp_consumed_weight += WRITE_CONSUMED_STEP;
        uint32_t max_value = write_max_consumed(app);
        if(app->temp_consumed_weight > max_value) app->temp_consumed_weight = max_value;
    }

    variable_item_set_current_value_index(item, WRITE_CONSUMED_CENTER_INDEX);
    write_update_items(app);
}

static void write_item_click_callback(void* context, uint32_t index) {
    OpenPrintTag* app = context;

    if(index == WriteItemConsumedWeight) {
        // OK on the total consumed weight opens the number pad to set the exact total
        view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventEditConsumed);
    } else if(index == WriteItemAddConsumption) {
        // OK on "Add consumption" opens the number pad for an amount to add to the total
        view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventAddConsumption);
    } else if(index == WriteItemSave) {
        // OK on "Save to Tag" saves
        view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventSave);
    }
}

static void write_number_input_callback(void* context, uint32_t number) {
    OpenPrintTag* app = context;

    if(app->write_number_input_additive) {
        // Never go above the most that can have been consumed
        uint32_t max_value = write_max_consumed(app);
        uint32_t room =
            max_value > app->temp_consumed_weight ? max_value - app->temp_consumed_weight : 0;
        app->temp_consumed_weight += number < room ? number : room;
    } else {
        app->temp_consumed_weight = number;
    }
    view_dispatcher_send_custom_event(app->view_dispatcher, WriteEventConsumedEntered);
}

// Encodes the auxiliary section, merges it into the tag's current memory and starts the write
static void openprinttag_write_start(OpenPrintTag* app) {
    if(!app->tag_data.main.has_data) {
        write_show_result(app, "Error", "No tag data\nto update", true);
        return;
    }

    uint8_t aux_buffer[512];
    size_t aux_size = openprinttag_encode_auxiliary(
        app, aux_buffer, sizeof(aux_buffer), app->temp_consumed_weight);
    if(aux_size == 0 || aux_size > app->tag_data.meta.aux_region_size) {
        write_show_result(app, "Error", "Failed to encode\nupdate data", true);
        return;
    }

    const Iso15693_3Data* iso_data = nfc_device_get_data(app->nfc_device, NfcProtocolIso15693_3);
    if(!iso_data) {
        write_show_result(app, "Error", "Tag data lost", true);
        return;
    }

    const uint8_t block_size = iso15693_3_get_block_size(iso_data);
    const uint16_t block_count = iso15693_3_get_block_count(iso_data);
    if(block_size == 0 || block_size > WRITE_MAX_BLOCK) {
        write_show_result(app, "Error", "Unsupported\nblock size", true);
        return;
    }

    // The auxiliary region offset is relative to the start of the NDEF payload
    const uint32_t aux_offset =
        app->tag_data.ndef_payload_offset + app->tag_data.meta.aux_region_offset;
    const uint16_t start_block = aux_offset / block_size;
    const uint8_t start_offset = aux_offset % block_size;
    const uint32_t region_size = app->tag_data.meta.aux_region_size;
    const uint16_t blocks_needed = (start_offset + region_size + block_size - 1) / block_size;

    if(start_block + blocks_needed > block_count ||
       start_block + blocks_needed > WRITE_MAX_BLOCK_COUNT) {
        write_show_result(app, "Error", "Not enough\ntag memory", true);
        return;
    }

    // Start from the tag's current blocks so bytes around the region are preserved
    const size_t padded_size = (size_t)blocks_needed * block_size;
    uint8_t* write_data = malloc(padded_size);
    if(!write_data) {
        write_show_result(app, "Error", "Out of memory", true);
        return;
    }

    for(uint16_t i = 0; i < blocks_needed; i++) {
        const uint8_t* block = iso15693_3_get_block_data(iso_data, start_block + i);
        if(!block) {
            free(write_data);
            write_show_result(app, "Error", "Tag data lost", true);
            return;
        }
        memcpy(write_data + (i * block_size), block, block_size);
    }

    // Clear the whole region so a shorter encoding leaves no stale bytes behind
    memset(write_data + start_offset, 0, region_size);
    memcpy(write_data + start_offset, aux_buffer, aux_size);

    write_free_data(app);
    app->write_data = write_data;
    app->write_data_size = padded_size;
    app->write_start_block = start_block;
    app->write_block_count = blocks_needed;
    app->write_current_block = 0;
    app->write_attempts = 0;
    app->write_in_progress = true;

    // Address the write to the tag that was read. The stored UID is reversed relative to the
    // order it is sent in.
    for(size_t i = 0; i < ISO15693_3_UID_SIZE; i++) {
        app->write_uid[i] = iso_data->uid[ISO15693_3_UID_SIZE - 1 - i];
    }

    FURI_LOG_I(
        TAG,
        "Writing %d blocks starting at block %d (aux CBOR %zu bytes)",
        blocks_needed,
        start_block,
        aux_size);

    // The tag does not have to be on the Flipper yet: the write starts as soon as it is held
    // near, and BACK returns to the edit screen with the entered value kept
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "Save to tag", 64, 6, AlignCenter, AlignTop);
    popup_set_text(
        popup, "Hold the same tag\nnear Flipper\n\nBACK = edit", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewPopup);

    // The read poller has already finished, replace it with the one that writes
    write_stop_poller(app);
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    nfc_poller_start_ex(app->nfc_poller, openprinttag_tag_write_callback, app);
}

void openprinttag_scene_write_on_enter(void* context) {
    OpenPrintTag* app = context;
    app->read_retries = 0;

    // Show popup with scanning message. The popup is reset first so no timeout, callback or
    // text from an earlier popup is left over.
    Popup* popup = app->popup;
    popup_reset(popup);
    popup_set_header(popup, "Scanning...", 64, 6, AlignCenter, AlignTop);
    popup_set_text(
        popup, "Place the tag\nnear Flipper\n\nBACK = cancel", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewPopup);

    // Allocate NFC device if not already allocated
    if(!app->nfc_device) {
        app->nfc_device = nfc_device_alloc();
    }

    // Start scanner
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->nfc_scanner, openprinttag_write_scanner_callback, app);
}

bool openprinttag_scene_write_on_event(void* context, SceneManagerEvent event) {
    OpenPrintTag* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WriteEventTagDetected) {
            // Tag detected, stop scanner and start poller
            nfc_scanner_stop(app->nfc_scanner);
            nfc_scanner_free(app->nfc_scanner);
            app->nfc_scanner = NULL;

            app->nfc_poller = nfc_poller_alloc(app->nfc, app->detected_protocol);
            nfc_poller_start(app->nfc_poller, openprinttag_write_poller_callback, app);
            consumed = true;
        } else if(event.event == WriteEventTagRead) {
            // Reading complete. The poller callback already returned NfcCommandStop; the poller
            // is stopped exactly once later by write_stop_poller() (a second nfc_poller_stop()
            // fails a furi_check inside nfc_stop and crashes the app).

            // A tag that was reported without blocks is read again with a new poller
            const OpenPrintTagReadCheck check =
                openprinttag_check_read(app, openprinttag_write_poller_callback);
            if(check == OpenPrintTagReadRetried) return true;
            if(check == OpenPrintTagReadFailed) {
                write_show_result(app, "Error", "Failed to read\nthe tag", false);
                return true;
            }

            // Get the ISO15693 data from the POLLER
            const NfcDeviceData* poller_data = nfc_poller_get_data(app->nfc_poller);
            const Iso15693_3Data* iso_data = (const Iso15693_3Data*)poller_data;

            if(iso_data) {
                // Keep a copy, the save step merges the update into these blocks
                nfc_device_set_data(app->nfc_device, NfcProtocolIso15693_3, iso_data);

                // Read all blocks
                uint16_t block_count = iso15693_3_get_block_count(iso_data);
                uint8_t block_size = iso15693_3_get_block_size(iso_data);

                size_t total_size = block_count * block_size;
                uint8_t* tag_memory = malloc(total_size);
                memset(tag_memory, 0, total_size);

                for(uint16_t i = 0; i < block_count; i++) {
                    const uint8_t* block = iso15693_3_get_block_data(iso_data, i);
                    if(block) {
                        memcpy(tag_memory + (i * block_size), block, block_size);
                    }
                }

                // Parse NDEF
                bool parse_success = openprinttag_parse_ndef(app, tag_memory, total_size);
                free(tag_memory);

                if(parse_success && app->tag_data.main.has_data) {
                    // Initialize temp value with current consumed weight
                    app->temp_consumed_weight = app->tag_data.aux.consumed_weight;

                    // Setup variable item list
                    VariableItemList* vil = app->variable_item_list;
                    variable_item_list_reset(vil);
                    variable_item_list_set_enter_callback(vil, write_item_click_callback, app);

                    // Rows, in the order of WriteItem: remaining (info), consumed, save
                    app->write_remaining_item =
                        variable_item_list_add(vil, "Remaining:", 1, NULL, NULL);

                    // Left/right changes the consumed weight in 20 g steps, OK opens the number
                    // keyboard for an exact value
                    app->write_consumed_item = variable_item_list_add(
                        vil, "Total consumed", 3, consumed_weight_change_callback, app);
                    variable_item_set_current_value_index(
                        app->write_consumed_item, WRITE_CONSUMED_CENTER_INDEX);

                    // OK opens the number pad for an amount that is added to the total
                    variable_item_list_add(vil, "Add consumption", 0, NULL, NULL);

                    variable_item_list_add(vil, "Save to Tag", 0, NULL, NULL);

                    write_update_items(app);

                    view_dispatcher_switch_to_view(
                        app->view_dispatcher, OpenPrintTagViewVariableItemList);
                } else {
                    // Parsing failed, go back
                    write_show_result(app, "Error", "Failed to read\nOpenPrintTag data", false);
                }
            } else {
                // Failed to read tag
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, OpenPrintTagSceneStart);
            }
            consumed = true;
        } else if(event.event == WriteEventSave) {
            openprinttag_write_start(app);
            consumed = true;
        } else if(event.event == WriteEventEditConsumed) {
            // The most that can have been consumed is a full spool
            numpad_setup(
                app->numpad,
                "Total consumed (g)",
                app->temp_consumed_weight,
                write_max_consumed(app),
                write_number_input_callback,
                app);
            app->write_number_input_additive = false;
            app->write_number_input_active = true;
            view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewNumberInput);
            consumed = true;
        } else if(event.event == WriteEventAddConsumption) {
            // Starts at 0, the entered amount is added to the current total
            uint32_t max_value = write_max_consumed(app);
            uint32_t room =
                max_value > app->temp_consumed_weight ? max_value - app->temp_consumed_weight : 0;
            numpad_setup(
                app->numpad, "Add consumption (g)", 0, room, write_number_input_callback, app);
            app->write_number_input_additive = true;
            app->write_number_input_active = true;
            view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewNumberInput);
            consumed = true;
        } else if(event.event == WriteEventConsumedEntered) {
            app->write_number_input_active = false;
            write_update_items(app);

            view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewVariableItemList);
            consumed = true;
        } else if(event.event == WriteEventWriteDone) {
            write_stop_poller(app);
            write_free_data(app);

            app->tag_data.aux.consumed_weight = app->temp_consumed_weight;
            app->tag_data.aux.has_data = true;

            write_show_result(app, "Saved", "Consumed weight\nupdated on tag", false);
            consumed = true;
        } else if(event.event == WriteEventWriteFailed) {
            FURI_LOG_E(
                TAG,
                "Write failed at block %d",
                app->write_start_block + app->write_current_block);
            write_stop_poller(app);
            write_free_data(app);

            // Back to the edit screen afterwards so the entered value is not lost
            write_show_result(app, "Write failed", "Tag is locked or\nrefuses the data", true);
            consumed = true;
        } else if(event.event == WriteEventResultDismissed) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, OpenPrintTagSceneStart);
            consumed = true;
        } else if(event.event == WriteEventFailureDismissed) {
            view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewVariableItemList);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack && app->write_number_input_active) {
        // Leave the number keyboard without changing the value
        app->write_number_input_active = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewVariableItemList);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack && app->write_in_progress) {
        // Cancel waiting for the tag and return to the edit screen, which still holds the value
        write_stop_poller(app);
        write_free_data(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewVariableItemList);
        consumed = true;
    }

    return consumed;
}

void openprinttag_scene_write_on_exit(void* context) {
    OpenPrintTag* app = context;

    if(app->nfc_scanner) {
        nfc_scanner_stop(app->nfc_scanner);
        nfc_scanner_free(app->nfc_scanner);
        app->nfc_scanner = NULL;
    }

    write_stop_poller(app);

    // Clean up write data if any
    write_free_data(app);

    variable_item_list_reset(app->variable_item_list);
    app->write_remaining_item = NULL;
    app->write_consumed_item = NULL;
    app->write_number_input_active = false;
    popup_reset(app->popup);
}
