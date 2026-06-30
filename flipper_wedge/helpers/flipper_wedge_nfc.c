#include "flipper_wedge_nfc.h"
#include <furi_hal.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <toolbox/simple_array.h>
#include <toolbox/bit_buffer.h>

#define TAG "FlipperWedgeNfc"

// Type 4 NDEF constants
#define NDEF_T4_AID_LEN 7
static const uint8_t NDEF_T4_AID[NDEF_T4_AID_LEN] = {
    0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01
};

#define NDEF_T4_FILE_ID_CC 0xE103      // Capability Container
#define NDEF_T4_FILE_ID_NDEF 0xE104    // NDEF Message

// APDU retry configuration
#define NDEF_T4_MAX_RETRIES 3          // Maximum retry attempts for APDU commands
#define NDEF_T4_RETRY_DELAY_MS 15      // Delay between retries in milliseconds

// APDU status codes
#define APDU_SW1_SUCCESS 0x90
#define APDU_SW2_SUCCESS 0x00

typedef enum {
    FlipperWedgeNfcStateIdle,
    FlipperWedgeNfcStateScanning,
    FlipperWedgeNfcStateTagDetected,  // Scanner detected tag, need to switch to poller
    FlipperWedgeNfcStatePolling,
    FlipperWedgeNfcStateSuccess,
    FlipperWedgeNfcStateError,  // Poller failed, need to restart scanner
} FlipperWedgeNfcState;

struct FlipperWedgeNfc {
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;

    FlipperWedgeNfcState state;
    bool parse_ndef;
    NfcProtocol detected_protocol;

    FlipperWedgeNfcCallback callback;
    void* callback_context;

    FlipperWedgeNfcData last_data;

    // Thread-safe signaling
    FuriThreadId owner_thread;
};

// Simple NDEF text record parser
// Returns number of bytes written to output, 0 if no text records found
// Parse raw NDEF records (for Type 4 tags - no TLV wrapping)
static size_t flipper_wedge_nfc_parse_raw_ndef_text(const uint8_t* data, size_t data_len, char* output, size_t output_max) {
    if(!data || !output || data_len < 4 || output_max == 0) {
        return 0;
    }

    size_t output_pos = 0;
    size_t pos = 0;

    // Parse NDEF message records directly (no TLV wrapper)
    while(pos < data_len) {
        // Read record header
        if(pos + 1 > data_len) break;
        uint8_t flags_tnf = data[pos++];

        uint8_t tnf = flags_tnf & 0x07;
        bool short_record = (flags_tnf & 0x10) != 0;
        bool id_length_present = (flags_tnf & 0x08) != 0;
        bool message_end = (flags_tnf & 0x40) != 0;

        // Read type length
        if(pos >= data_len) break;
        uint8_t type_len = data[pos++];

        // Read payload length
        if(pos >= data_len) break;
        uint32_t payload_len;
        if(short_record) {
            payload_len = data[pos++];
        } else {
            if(pos + 4 > data_len) break;
            payload_len = (data[pos] << 24) | (data[pos + 1] << 16) |
                         (data[pos + 2] << 8) | data[pos + 3];
            pos += 4;
        }

        // Read ID length if present
        uint8_t id_len = 0;
        if(id_length_present) {
            if(pos >= data_len) break;
            id_len = data[pos++];
        }

        // Read type
        if(pos + type_len > data_len) break;
        const uint8_t* type = &data[pos];
        pos += type_len;

        // Skip ID
        if(pos + id_len > data_len) break;
        pos += id_len;

        // Read payload
        if(pos + payload_len > data_len) break;
        const uint8_t* payload = &data[pos];
        pos += payload_len;

        // Check if this is a text record (TNF=0x01, Type='T')
        if(tnf == 0x01 && type_len == 1 && type[0] == 'T' && payload_len > 1) {
            FURI_LOG_I(TAG, "Type 4 NDEF: Found text record in raw NDEF");

            // Text record format: [status byte][language code][text]
            uint8_t status = payload[0];
            uint8_t lang_len = status & 0x3F;

            FURI_LOG_I(TAG, "Type 4 NDEF: Status=0x%02X, lang_len=%d, payload_len=%lu",
                       status, lang_len, payload_len);

            if((uint32_t)(lang_len + 1) <= payload_len) {
                // Skip language code, extract text
                const uint8_t* text = &payload[1 + lang_len];
                size_t text_len = payload_len - 1 - lang_len;

                FURI_LOG_I(TAG, "Type 4 NDEF: Text length=%zu", text_len);

                // Copy text to output
                size_t copy_len = text_len;
                if(output_pos + copy_len >= output_max) {
                    copy_len = output_max - output_pos - 1;
                }
                if(copy_len > 0) {
                    memcpy(&output[output_pos], text, copy_len);
                    output_pos += copy_len;
                }
            }
        }

        // Stop if this was the last record
        if(message_end) break;
    }

    // Null-terminate
    if(output_pos < output_max) {
        output[output_pos] = '\0';
    } else if(output_max > 0) {
        output[output_max - 1] = '\0';
    }

    FURI_LOG_I(TAG, "Type 4 NDEF: Parsed %zu bytes of text", output_pos);
    return output_pos;
}

// Parse TLV-wrapped NDEF records (for Type 2/5 tags)
static size_t flipper_wedge_nfc_parse_ndef_text(const uint8_t* data, size_t data_len, char* output, size_t output_max) {
    if(!data || !output || data_len < 4 || output_max == 0) {
        return 0;
    }

    size_t output_pos = 0;
    size_t pos = 0;

    // Look for NDEF Message TLV (Type=0x03)
    while(pos < data_len - 1) {
        uint8_t tlv_type = data[pos++];

        // Skip padding
        if(tlv_type == 0x00) continue;

        // Terminator found
        if(tlv_type == 0xFE) break;

        // Get length
        if(pos >= data_len) break;
        uint32_t tlv_len = data[pos++];
        if(tlv_len == 0xFF) {
            // 3-byte length
            if(pos + 2 > data_len) break;
            tlv_len = (data[pos] << 8) | data[pos + 1];
            pos += 2;
        }

        // Check if this is an NDEF message
        if(tlv_type == 0x03 && tlv_len > 0 && pos + tlv_len <= data_len) {
            // Parse NDEF message records
            size_t msg_end = pos + tlv_len;

            while(pos < msg_end && pos < data_len) {
                // Read record header
                if(pos + 1 > data_len) break;
                uint8_t flags_tnf = data[pos++];

                uint8_t tnf = flags_tnf & 0x07;
                bool short_record = (flags_tnf & 0x10) != 0;
                bool id_length_present = (flags_tnf & 0x08) != 0;

                // Read type length
                if(pos >= data_len) break;
                uint8_t type_len = data[pos++];

                // Read payload length
                if(pos >= data_len) break;
                uint32_t payload_len;
                if(short_record) {
                    payload_len = data[pos++];
                } else {
                    if(pos + 4 > data_len) break;
                    payload_len = (data[pos] << 24) | (data[pos + 1] << 16) |
                                 (data[pos + 2] << 8) | data[pos + 3];
                    pos += 4;
                }

                // Read ID length if present
                uint8_t id_len = 0;
                if(id_length_present) {
                    if(pos >= data_len) break;
                    id_len = data[pos++];
                }

                // Read type
                if(pos + type_len > data_len) break;
                const uint8_t* type = &data[pos];
                pos += type_len;

                // Skip ID
                if(pos + id_len > data_len) break;
                pos += id_len;

                // Read payload
                if(pos + payload_len > data_len) break;
                const uint8_t* payload = &data[pos];
                pos += payload_len;

                // Check if this is a text record (TNF=0x01, Type='T')
                if(tnf == 0x01 && type_len == 1 && type[0] == 'T' && payload_len > 1) {
                    // Text record format: [status byte][language code][text]
                    uint8_t status = payload[0];
                    uint8_t lang_len = status & 0x3F;

                    if((uint32_t)(lang_len + 1) <= payload_len) {
                        // Skip language code, extract text
                        const uint8_t* text = &payload[1 + lang_len];
                        size_t text_len = payload_len - 1 - lang_len;

                        // Copy text to output
                        size_t copy_len = text_len;
                        if(output_pos + copy_len >= output_max) {
                            copy_len = output_max - output_pos - 1;
                        }
                        if(copy_len > 0) {
                            memcpy(&output[output_pos], text, copy_len);
                            output_pos += copy_len;
                        }
                    }
                }
            }

            // Found and processed NDEF message, stop searching
            break;
        } else {
            // Skip this TLV
            pos += tlv_len;
        }
    }

    // Null-terminate
    if(output_pos < output_max) {
        output[output_pos] = '\0';
    } else if(output_max > 0) {
        output[output_max - 1] = '\0';
    }

    return output_pos;
}

// Type 4 NDEF APDU Helper Functions

// Check if APDU response has success status (90 00)
static bool flipper_wedge_nfc_t4_check_apdu_success(const BitBuffer* rx_buffer) {
    size_t resp_len = bit_buffer_get_size_bytes(rx_buffer);
    if(resp_len < 2) return false;

    uint8_t sw1 = bit_buffer_get_byte(rx_buffer, resp_len - 2);
    uint8_t sw2 = bit_buffer_get_byte(rx_buffer, resp_len - 1);

    return (sw1 == APDU_SW1_SUCCESS && sw2 == APDU_SW2_SUCCESS);
}

// Build SELECT by AID APDU command
static void flipper_wedge_nfc_t4_build_select_app_apdu(BitBuffer* tx_buffer) {
    bit_buffer_reset(tx_buffer);
    bit_buffer_append_byte(tx_buffer, 0x00);  // CLA
    bit_buffer_append_byte(tx_buffer, 0xA4);  // INS (SELECT)
    bit_buffer_append_byte(tx_buffer, 0x04);  // P1 (select by name/AID)
    bit_buffer_append_byte(tx_buffer, 0x00);  // P2
    bit_buffer_append_byte(tx_buffer, NDEF_T4_AID_LEN);  // Lc (data length)
    bit_buffer_append_bytes(tx_buffer, NDEF_T4_AID, NDEF_T4_AID_LEN);  // AID data
    // Note: Some Type 4 tags don't require Le for SELECT, trying without it first
}

// Build SELECT by File ID APDU command
static void flipper_wedge_nfc_t4_build_select_file_apdu(BitBuffer* tx_buffer, uint16_t file_id) {
    bit_buffer_reset(tx_buffer);
    bit_buffer_append_byte(tx_buffer, 0x00);  // CLA
    bit_buffer_append_byte(tx_buffer, 0xA4);  // INS (SELECT)
    bit_buffer_append_byte(tx_buffer, 0x00);  // P1 (select by file ID)
    bit_buffer_append_byte(tx_buffer, 0x0C);  // P2 (first or only occurrence)
    bit_buffer_append_byte(tx_buffer, 0x02);  // Lc (2 bytes for file ID)
    bit_buffer_append_byte(tx_buffer, (file_id >> 8) & 0xFF);  // File ID MSB
    bit_buffer_append_byte(tx_buffer, file_id & 0xFF);  // File ID LSB
    // Note: No Le needed for SELECT file
}

// Build READ BINARY APDU command
static void flipper_wedge_nfc_t4_build_read_binary_apdu(
    BitBuffer* tx_buffer,
    uint16_t offset,
    uint8_t length) {
    bit_buffer_reset(tx_buffer);
    bit_buffer_append_byte(tx_buffer, 0x00);  // CLA
    bit_buffer_append_byte(tx_buffer, 0xB0);  // INS (READ BINARY)
    bit_buffer_append_byte(tx_buffer, (offset >> 8) & 0xFF);  // P1 (offset MSB)
    bit_buffer_append_byte(tx_buffer, offset & 0xFF);  // P2 (offset LSB)
    bit_buffer_append_byte(tx_buffer, length);  // Le (bytes to read)
}

// Read Type 4 NDEF data from ISO14443-4A tag
static bool flipper_wedge_nfc_read_type4_ndef(
    Iso14443_4aPoller* poller,
    FlipperWedgeNfcData* data) {

    BitBuffer* tx_buffer = bit_buffer_alloc(256);
    BitBuffer* rx_buffer = bit_buffer_alloc(256);
    bool success = false;

    FURI_LOG_I(TAG, "========== Type 4 NDEF: Starting NDEF read sequence ==========");

    do {
        // Step 1: SELECT NDEF Application (with retry logic)
        FURI_LOG_I(TAG, "Type 4 NDEF: Step 1 - SELECT NDEF Application (AID: D2760000850101)");

        Iso14443_4aError error = Iso14443_4aErrorNone;
        bool select_success = false;

        for(uint8_t retry = 0; retry < NDEF_T4_MAX_RETRIES; retry++) {
            if(retry > 0) {
                FURI_LOG_I(TAG, "Type 4 NDEF: Retry attempt %d/%d after %dms delay",
                           retry + 1, NDEF_T4_MAX_RETRIES, NDEF_T4_RETRY_DELAY_MS);
                furi_delay_ms(NDEF_T4_RETRY_DELAY_MS);
            }

            flipper_wedge_nfc_t4_build_select_app_apdu(tx_buffer);
            error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

            if(error == Iso14443_4aErrorNone) {
                // Log response for debugging
                size_t resp_len = bit_buffer_get_size_bytes(rx_buffer);
                FURI_LOG_I(TAG, "Type 4 NDEF: SELECT app response length: %zu bytes", resp_len);
                if(resp_len >= 2) {
                    uint8_t sw1 = bit_buffer_get_byte(rx_buffer, resp_len - 2);
                    uint8_t sw2 = bit_buffer_get_byte(rx_buffer, resp_len - 1);
                    FURI_LOG_I(TAG, "Type 4 NDEF: SELECT app status: SW1=%02X SW2=%02X", sw1, sw2);
                } else {
                    FURI_LOG_E(TAG, "Type 4 NDEF: Response too short!");
                }

                if(flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
                    select_success = true;
                    FURI_LOG_I(TAG, "Type 4 NDEF: NDEF application selected successfully");
                    break;
                } else {
                    // Application not found (APDU status error) - don't retry
                    FURI_LOG_W(TAG, "Type 4 NDEF: No NDEF application found (invalid APDU status)");
                    break;
                }
            } else {
                // Communication error - will retry if attempts remain
                FURI_LOG_W(TAG, "Type 4 NDEF: SELECT app failed, error=%d", error);
            }
        }

        if(!select_success) {
            // Type 4 tag detected but no NDEF app found
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        // Step 2: SELECT Capability Container (CC) file
        FURI_LOG_I(TAG, "Type 4 NDEF: Step 2 - SELECT CC file (0xE103)");
        flipper_wedge_nfc_t4_build_select_file_apdu(tx_buffer, NDEF_T4_FILE_ID_CC);
        error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

        if(error != Iso14443_4aErrorNone || !flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
            FURI_LOG_W(TAG, "Type 4 NDEF: SELECT CC file failed");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        FURI_LOG_I(TAG, "Type 4 NDEF: CC file selected successfully");

        // Step 3: READ CC file (first 15 bytes to get structure)
        FURI_LOG_I(TAG, "Type 4 NDEF: Step 3 - READ CC file");
        flipper_wedge_nfc_t4_build_read_binary_apdu(tx_buffer, 0, 15);
        error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

        if(error != Iso14443_4aErrorNone || !flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
            FURI_LOG_W(TAG, "Type 4 NDEF: READ CC file failed");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        size_t cc_len = bit_buffer_get_size_bytes(rx_buffer) - 2; // Subtract SW1 SW2
        if(cc_len < 15) {
            FURI_LOG_W(TAG, "Type 4 NDEF: CC too short (%zu bytes)", cc_len);
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        // Parse and validate CC file structure
        // Type 4 CC format:
        // Bytes 0-1: CCLEN (CC file length)
        // Byte 2: Mapping Version
        // Bytes 3-4: MLe (max R-APDU data size)
        // Bytes 5-6: MLc (max C-APDU data size)
        // Byte 7+: TLV blocks

        uint16_t cc_file_len = (bit_buffer_get_byte(rx_buffer, 0) << 8) |
                               bit_buffer_get_byte(rx_buffer, 1);
        uint8_t mapping_version = bit_buffer_get_byte(rx_buffer, 2);

        FURI_LOG_I(TAG, "Type 4 NDEF: CC length=%d, version=0x%02X", cc_file_len, mapping_version);

        // Validate mapping version (should be 0x10, 0x20, or 0x30)
        if(mapping_version < 0x10 || mapping_version > 0x30) {
            FURI_LOG_W(TAG, "Type 4 NDEF: Invalid mapping version 0x%02X", mapping_version);
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        FURI_LOG_I(TAG, "Type 4 NDEF: Valid CC found");

        // Step 4: SELECT NDEF Message file
        flipper_wedge_nfc_t4_build_select_file_apdu(tx_buffer, NDEF_T4_FILE_ID_NDEF);
        error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

        if(error != Iso14443_4aErrorNone || !flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
            FURI_LOG_W(TAG, "Type 4 NDEF: SELECT NDEF file failed");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        FURI_LOG_D(TAG, "Type 4 NDEF: NDEF file selected");

        // Step 5: READ NDEF length (first 2 bytes)
        flipper_wedge_nfc_t4_build_read_binary_apdu(tx_buffer, 0, 2);
        error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

        if(error != Iso14443_4aErrorNone || !flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
            FURI_LOG_W(TAG, "Type 4 NDEF: READ NDEF length failed");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        if(bit_buffer_get_size_bytes(rx_buffer) < 4) { // 2 length bytes + SW1 SW2
            FURI_LOG_W(TAG, "Type 4 NDEF: NDEF length response too short");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        uint16_t ndef_len = (bit_buffer_get_byte(rx_buffer, 0) << 8) |
                            bit_buffer_get_byte(rx_buffer, 1);

        if(ndef_len == 0) {
            FURI_LOG_I(TAG, "Type 4 NDEF: Empty NDEF message");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        // Limit NDEF read to reasonable size (increased from 240 to support large text records)
        if(ndef_len > 1024) {
            FURI_LOG_W(TAG, "Type 4 NDEF: NDEF too large (%d bytes), limiting to 1024", ndef_len);
            ndef_len = 1024;
        }

        FURI_LOG_D(TAG, "Type 4 NDEF: NDEF length = %d bytes", ndef_len);

        // Step 6: READ NDEF Message data (skip 2-byte length prefix)
        // Read in chunks if needed (most tags support up to 128-250 bytes per read)
        uint8_t ndef_data[1024];
        uint16_t bytes_read = 0;

        while(bytes_read < ndef_len) {
            uint8_t chunk_size = (ndef_len - bytes_read > 128) ? 128 : (ndef_len - bytes_read);

            flipper_wedge_nfc_t4_build_read_binary_apdu(tx_buffer, 2 + bytes_read, chunk_size);
            error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);

            if(error != Iso14443_4aErrorNone || !flipper_wedge_nfc_t4_check_apdu_success(rx_buffer)) {
                FURI_LOG_W(TAG, "Type 4 NDEF: READ NDEF chunk failed at offset %d", bytes_read);
                break;
            }

            size_t chunk_received = bit_buffer_get_size_bytes(rx_buffer) - 2; // Subtract SW1 SW2
            if(chunk_received == 0) {
                FURI_LOG_W(TAG, "Type 4 NDEF: No data in chunk");
                break;
            }

            // Copy chunk to buffer
            for(size_t i = 0; i < chunk_received && bytes_read < ndef_len; i++) {
                ndef_data[bytes_read++] = bit_buffer_get_byte(rx_buffer, i);
            }

            FURI_LOG_D(TAG, "Type 4 NDEF: Read %zu bytes, total %d/%d", chunk_received, bytes_read, ndef_len);
        }

        if(bytes_read == 0) {
            FURI_LOG_W(TAG, "Type 4 NDEF: No NDEF data read");
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            break;
        }

        FURI_LOG_I(TAG, "Type 4 NDEF: Successfully read %d bytes", bytes_read);

        // Log the raw NDEF data for debugging
        FURI_LOG_I(TAG, "Type 4 NDEF: Raw data (first %d bytes):", bytes_read > 32 ? 32 : bytes_read);
        for(uint16_t i = 0; i < bytes_read && i < 32; i++) {
            FURI_LOG_I(TAG, "  [%02d] = 0x%02X", i, ndef_data[i]);
        }

        // Step 7: Parse NDEF message to extract text records
        // Type 4 uses raw NDEF records (no TLV wrapping)
        size_t text_len = flipper_wedge_nfc_parse_raw_ndef_text(
            ndef_data,
            bytes_read,
            data->ndef_text,
            FLIPPER_WEDGE_NDEF_MAX_LEN);

        if(text_len > 0) {
            data->has_ndef = true;
            data->error = FlipperWedgeNfcErrorNone;
            success = true;
            FURI_LOG_I(TAG, "Type 4 NDEF: Found text record: %s", data->ndef_text);
        } else {
            data->error = FlipperWedgeNfcErrorNoTextRecord;
            FURI_LOG_D(TAG, "Type 4 NDEF: No text records found in NDEF message");
        }

    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return success;
}

static NfcCommand flipper_wedge_nfc_poller_callback_iso14443_3a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipperWedgeNfc* instance = context;

    FURI_LOG_I(TAG, "========== ISO14443-3A CALLBACK INVOKED ==========");
    FURI_LOG_I(TAG, "3A callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* iso3a_event = event.event_data;
        FURI_LOG_I(TAG, "3A event type: %d", iso3a_event->type);

        if(iso3a_event->type == Iso14443_3aPollerEventTypeReady) {
            FURI_LOG_I(TAG, "3A poller event: READY - tag is activated");
            const Iso14443_3aData* iso3a_data = nfc_poller_get_data(instance->poller);
            FURI_LOG_I(TAG, "3A data ptr: %p", (void*)iso3a_data);

            if(iso3a_data) {
                // Validate UID length first
                uint8_t uid_len = iso3a_data->uid_len;
                FURI_LOG_I(TAG, "3A UID length from data: %d", uid_len);

                if(uid_len > FLIPPER_WEDGE_NFC_UID_MAX_LEN) {
                    FURI_LOG_W(TAG, "3A UID length %d exceeds max %d, truncating", uid_len, FLIPPER_WEDGE_NFC_UID_MAX_LEN);
                    uid_len = FLIPPER_WEDGE_NFC_UID_MAX_LEN;
                }
                if(uid_len > 0) {
                    instance->last_data.uid_len = uid_len;
                    memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                    instance->last_data.has_ndef = false;
                    instance->last_data.ndef_text[0] = '\0';

                    // ISO14443-3A doesn't support NDEF - if NDEF was requested, mark as not forum compliant
                    if(instance->parse_ndef) {
                        instance->last_data.error = FlipperWedgeNfcErrorNotForumCompliant;
                        FURI_LOG_I(TAG, "Got ISO14443-3A UID (not NFC Forum compliant), len: %d", instance->last_data.uid_len);
                    } else {
                        instance->last_data.error = FlipperWedgeNfcErrorNone;
                        FURI_LOG_I(TAG, "Got ISO14443-3A UID, len: %d", instance->last_data.uid_len);
                    }
                    instance->state = FlipperWedgeNfcStateSuccess;
                } else {
                    FURI_LOG_E(TAG, "3A UID length is 0, cannot proceed");
                    instance->state = FlipperWedgeNfcStateError;
                }
            } else {
                FURI_LOG_E(TAG, "3A poller returned NULL data");
                instance->state = FlipperWedgeNfcStateError;
            }
            return NfcCommandStop;
        } else if(iso3a_event->type == Iso14443_3aPollerEventTypeError) {
            FURI_LOG_E(TAG, "3A poller event: ERROR - activation or communication failed");
            FURI_LOG_E(TAG, "3A error: Check if tag is still present and properly positioned");
            instance->state = FlipperWedgeNfcStateError;
            return NfcCommandStop;
        } else {
            FURI_LOG_W(TAG, "3A poller event: UNKNOWN type %d", iso3a_event->type);
        }
    } else {
        FURI_LOG_W(TAG, "3A callback received unexpected protocol: %d", event.protocol);
    }
    return NfcCommandContinue;
}

static NfcCommand flipper_wedge_nfc_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipperWedgeNfc* instance = context;

    FURI_LOG_I(TAG, "========== ISO14443-4A CALLBACK INVOKED ==========");
    FURI_LOG_I(TAG, "4A callback: protocol=%d", event.protocol);

    // ISO14443-4A pollers receive both 3A and 4A events
    // We only care about the 4A Ready event which means the full handshake is done
    if(event.protocol == NfcProtocolIso14443_4a) {
        const Iso14443_4aPollerEvent* iso4a_event = event.event_data;
        FURI_LOG_D(TAG, "4A event type: %d", iso4a_event->type);

        if(iso4a_event->type == Iso14443_4aPollerEventTypeReady) {
            FURI_LOG_I(TAG, "4A poller event: READY - tag is activated");
            const Iso14443_4aData* iso4a_data = nfc_poller_get_data(instance->poller);
            FURI_LOG_I(TAG, "4A data ptr: %p", (void*)iso4a_data);

            if(iso4a_data) {
                // Get the 3a base data which contains the UID
                const Iso14443_3aData* iso3a_data = iso4a_data->iso14443_3a_data;
                FURI_LOG_D(TAG, "3A data ptr: %p", (void*)iso3a_data);

                if(iso3a_data) {
                    // Validate UID length first
                    uint8_t uid_len = iso3a_data->uid_len;
                    FURI_LOG_D(TAG, "UID len: %d", uid_len);

                    if(uid_len > FLIPPER_WEDGE_NFC_UID_MAX_LEN) {
                        uid_len = FLIPPER_WEDGE_NFC_UID_MAX_LEN;
                    }
                    if(uid_len > 0) {
                        instance->last_data.uid_len = uid_len;
                        memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                        instance->last_data.has_ndef = false;
                        instance->last_data.ndef_text[0] = '\0';
                        instance->last_data.error = FlipperWedgeNfcErrorNone;

                        // ISO14443-4A is Type 4 NDEF - ALWAYS try to read NDEF
                        FURI_LOG_I(TAG, "Got ISO14443-4A UID, len: %d, attempting Type 4 NDEF read", instance->last_data.uid_len);

                        // Attempt to read Type 4 NDEF data
                        Iso14443_4aPoller* iso4a_poller = event.instance;
                        flipper_wedge_nfc_read_type4_ndef(iso4a_poller, &instance->last_data);

                        // flipper_wedge_nfc_read_type4_ndef sets error field:
                        // - FlipperWedgeNfcErrorNone if NDEF text found
                        // - FlipperWedgeNfcErrorUnsupportedType if no NDEF app
                        // - FlipperWedgeNfcErrorNoTextRecord if NDEF exists but no text records

                        // If we're NOT in NDEF-only mode, we still want to output UID even if NDEF fails
                        if(!instance->parse_ndef) {
                            // NFC mode: UID is always valid, NDEF is optional
                            if(instance->last_data.error != FlipperWedgeNfcErrorNone) {
                                FURI_LOG_I(TAG, "Type 4 NDEF parsing failed, will output UID only");
                            }
                            instance->last_data.error = FlipperWedgeNfcErrorNone;
                        }
                        // If parse_ndef is true (NDEF mode), keep the error as-is

                        instance->state = FlipperWedgeNfcStateSuccess;
                    }
                } else {
                    FURI_LOG_E(TAG, "4A data has NULL 3A pointer");
                    instance->state = FlipperWedgeNfcStateError;
                }
            } else {
                FURI_LOG_E(TAG, "4A poller returned NULL data");
                instance->state = FlipperWedgeNfcStateError;
            }
            return NfcCommandStop;
        } else if(iso4a_event->type == Iso14443_4aPollerEventTypeError) {
            FURI_LOG_E(TAG, "4A poller error");
            instance->state = FlipperWedgeNfcStateError;
            return NfcCommandStop;
        }
    } else if(event.protocol == NfcProtocolIso14443_3a) {
        // Ignore 3A events from the 4A poller - just continue
        FURI_LOG_D(TAG, "4A poller got 3A event, continuing...");
    }
    return NfcCommandContinue;
}

static NfcCommand flipper_wedge_nfc_poller_callback_mf_ultralight(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipperWedgeNfc* instance = context;

    FURI_LOG_I(TAG, "========== MF ULTRALIGHT CALLBACK INVOKED ==========");
    FURI_LOG_I(TAG, "MF Ultralight callback: protocol=%d, parse_ndef=%d", event.protocol, instance->parse_ndef);

    if(event.protocol == NfcProtocolMfUltralight) {
        const MfUltralightPollerEvent* mfu_event = event.event_data;
        FURI_LOG_I(TAG, "MFU event type: %d", mfu_event->type);

        if(mfu_event->type == MfUltralightPollerEventTypeReadSuccess) {
            FURI_LOG_I(TAG, "MFU poller event: READ SUCCESS");
            // Successfully read the tag
            const MfUltralightData* mfu_data = nfc_poller_get_data(instance->poller);
            FURI_LOG_I(TAG, "MFU data ptr: %p", (void*)mfu_data);

            if(mfu_data) {
                FURI_LOG_I(TAG, "MFU pages_read: %d", mfu_data->pages_read);
                // Get UID from ISO14443-3A base data
                const Iso14443_3aData* iso3a_data = mfu_data->iso14443_3a_data;
                FURI_LOG_I(TAG, "MFU 3A data ptr: %p", (void*)iso3a_data);

                if(iso3a_data) {
                    uint8_t uid_len = iso3a_data->uid_len;
                    FURI_LOG_I(TAG, "MFU UID length: %d", uid_len);

                    if(uid_len > FLIPPER_WEDGE_NFC_UID_MAX_LEN) {
                        FURI_LOG_W(TAG, "MFU UID length %d exceeds max, truncating", uid_len);
                        uid_len = FLIPPER_WEDGE_NFC_UID_MAX_LEN;
                    }
                    if(uid_len > 0) {
                        instance->last_data.uid_len = uid_len;
                        memcpy(instance->last_data.uid, iso3a_data->uid, uid_len);
                        instance->last_data.has_ndef = false;
                        instance->last_data.ndef_text[0] = '\0';
                        instance->last_data.error = FlipperWedgeNfcErrorNone;

                        FURI_LOG_I(TAG, "Got MF Ultralight UID, len: %d", instance->last_data.uid_len);

                        // Parse NDEF if requested
                        if(instance->parse_ndef && mfu_data->pages_read > 4) {
                            // NDEF data typically starts at page 4 (byte offset 16)
                            // Pages 0-3 are reserved for UID and lock bytes
                            // Increased limit from 240 to 1024 to support large text records
                            size_t ndef_data_len = (mfu_data->pages_read - 4) * 4;
                            if(ndef_data_len > 1024) {
                                FURI_LOG_W(TAG, "Type 2 NDEF: Data too large (%zu bytes), limiting to 1024", ndef_data_len);
                                ndef_data_len = 1024;
                            }

                            const uint8_t* ndef_data = &mfu_data->page[4].data[0];

                            FURI_LOG_I(TAG, "Attempting NDEF parse, data_len=%zu, pages_read=%d",
                                      ndef_data_len, mfu_data->pages_read);

                            size_t text_len = flipper_wedge_nfc_parse_ndef_text(
                                ndef_data,
                                ndef_data_len,
                                instance->last_data.ndef_text,
                                FLIPPER_WEDGE_NDEF_MAX_LEN);

                            if(text_len > 0) {
                                instance->last_data.has_ndef = true;
                                instance->last_data.error = FlipperWedgeNfcErrorNone;
                                FURI_LOG_I(TAG, "Found NDEF text: %s", instance->last_data.ndef_text);
                            } else {
                                // Type 2 tag but no NDEF text record found
                                instance->last_data.error = FlipperWedgeNfcErrorNoTextRecord;
                                FURI_LOG_I(TAG, "No NDEF text records found on Type 2 tag");
                            }
                        } else if(instance->parse_ndef) {
                            // Not enough pages read for NDEF
                            instance->last_data.error = FlipperWedgeNfcErrorNoTextRecord;
                            FURI_LOG_I(TAG, "Not enough pages for NDEF (pages_read=%d)", mfu_data->pages_read);
                        } else {
                            FURI_LOG_I(TAG, "NDEF parsing not requested (parse_ndef=false)");
                        }

                        instance->state = FlipperWedgeNfcStateSuccess;
                    } else {
                        FURI_LOG_E(TAG, "MFU UID length is 0");
                        instance->state = FlipperWedgeNfcStateError;
                    }
                } else {
                    FURI_LOG_E(TAG, "MFU data has NULL 3A pointer");
                    instance->state = FlipperWedgeNfcStateError;
                }
            } else {
                FURI_LOG_E(TAG, "MFU poller returned NULL data");
                instance->state = FlipperWedgeNfcStateError;
            }
            return NfcCommandStop;
        } else if(mfu_event->type == MfUltralightPollerEventTypeReadFailed) {
            FURI_LOG_E(TAG, "MFU poller event: READ FAILED");
            FURI_LOG_E(TAG, "MFU read failed - tag may have been removed or communication error occurred");
            instance->state = FlipperWedgeNfcStateError;
            return NfcCommandStop;
        } else if(mfu_event->type == MfUltralightPollerEventTypeRequestMode) {
            // Set read mode
            mfu_event->data->poller_mode = MfUltralightPollerModeRead;
            FURI_LOG_I(TAG, "MFU poller event: REQUEST MODE - set to read mode");
            return NfcCommandContinue;
        } else {
            FURI_LOG_W(TAG, "MFU poller event: UNKNOWN type %d", mfu_event->type);
        }
    } else {
        FURI_LOG_W(TAG, "MFU callback received unexpected protocol: %d", event.protocol);
    }
    return NfcCommandContinue;
}

static NfcCommand flipper_wedge_nfc_poller_callback_iso15693(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipperWedgeNfc* instance = context;

    FURI_LOG_D(TAG, "ISO15693 callback: protocol=%d", event.protocol);

    if(event.protocol == NfcProtocolIso15693_3) {
        const Iso15693_3PollerEvent* iso15_event = event.event_data;
        FURI_LOG_D(TAG, "ISO15693 event type: %d", iso15_event->type);

        if(iso15_event->type == Iso15693_3PollerEventTypeReady) {
            const Iso15693_3Data* iso15_data = nfc_poller_get_data(instance->poller);
            if(iso15_data) {
                // ISO15693 UID is 8 bytes
                uint8_t uid_len = 8;
                if(uid_len > FLIPPER_WEDGE_NFC_UID_MAX_LEN) {
                    uid_len = FLIPPER_WEDGE_NFC_UID_MAX_LEN;
                }

                instance->last_data.uid_len = uid_len;
                memcpy(instance->last_data.uid, iso15_data->uid, uid_len);
                instance->last_data.has_ndef = false;
                instance->last_data.ndef_text[0] = '\0';
                instance->last_data.error = FlipperWedgeNfcErrorNone;

                FURI_LOG_I(TAG, "Got ISO15693 UID, len: %d", instance->last_data.uid_len);

                // Type 5 NDEF parsing - use block data already read by poller
                if(instance->parse_ndef && iso15_data->block_data) {
                    FURI_LOG_D(TAG, "Attempting Type 5 NDEF parsing");

                    // Get system info from the data structure
                    uint16_t block_count = iso15_data->system_info.block_count;
                    uint8_t block_size = iso15_data->system_info.block_size;

                    FURI_LOG_D(TAG, "System info: block_count=%d, block_size=%d", block_count, block_size);

                    // Check if we have block data
                    const uint8_t* block_data = simple_array_cget_data(iso15_data->block_data);
                    size_t block_data_size = simple_array_get_count(iso15_data->block_data);

                    if(block_data && block_data_size >= 4) {
                        FURI_LOG_D(TAG, "Block data available, size=%zu bytes", block_data_size);

                        // Validate Capability Container (CC) at block 0
                        // CC format: [Magic 0xE1][Version/Access][MLEN][Additional features]
                        if(block_data[0] == 0xE1) {
                            FURI_LOG_D(TAG, "Valid CC found (magic=0xE1), version=0x%02X", block_data[1]);

                            // NDEF data starts after CC (4 bytes)
                            size_t ndef_data_len = block_data_size - 4;

                            // Limit to reasonable size (increased from 256 to 1024 for large text records)
                            if(ndef_data_len > 1024) {
                                FURI_LOG_W(TAG, "Type 5 NDEF: Data too large (%zu bytes), limiting to 1024", ndef_data_len);
                                ndef_data_len = 1024;
                            }

                            size_t text_len = flipper_wedge_nfc_parse_ndef_text(
                                &block_data[4], // Skip 4-byte CC
                                ndef_data_len,
                                instance->last_data.ndef_text,
                                FLIPPER_WEDGE_NDEF_MAX_LEN);

                            if(text_len > 0) {
                                instance->last_data.has_ndef = true;
                                instance->last_data.error = FlipperWedgeNfcErrorNone;
                                FURI_LOG_I(TAG, "Found Type 5 NDEF text: %s", instance->last_data.ndef_text);
                            } else {
                                // Type 5 tag with valid CC but no NDEF text record
                                instance->last_data.error = FlipperWedgeNfcErrorNoTextRecord;
                                FURI_LOG_D(TAG, "No NDEF text records found on Type 5 tag");
                            }
                        } else {
                            // No valid Capability Container
                            instance->last_data.error = FlipperWedgeNfcErrorNoTextRecord;
                            FURI_LOG_D(TAG, "Invalid CC magic: 0x%02X (expected 0xE1)", block_data[0]);
                        }
                    } else {
                        FURI_LOG_W(TAG, "No block data available or insufficient size");
                        instance->last_data.error = FlipperWedgeNfcErrorNoTextRecord;
                    }
                }

                instance->state = FlipperWedgeNfcStateSuccess;
            } else {
                FURI_LOG_E(TAG, "ISO15693 poller returned NULL data");
                instance->state = FlipperWedgeNfcStateError;
            }
            return NfcCommandStop;
        } else if(iso15_event->type == Iso15693_3PollerEventTypeError) {
            FURI_LOG_E(TAG, "ISO15693 poller error");
            instance->state = FlipperWedgeNfcStateError;
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

static void flipper_wedge_nfc_scanner_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    FlipperWedgeNfc* instance = context;

    if(event.type == NfcScannerEventTypeDetected) {
        FURI_LOG_I(TAG, "========== NFC TAG DETECTED ==========");
        FURI_LOG_I(TAG, "NFC tag detected, number of protocols: %zu", event.data.protocol_num);

        // Select best protocol in priority order (NDEF capability is handled in callbacks)
        // Priority: MfUltralight > ISO14443-4A > ISO15693 > ISO14443-3A
        NfcProtocol protocol_to_use = NfcProtocolInvalid;

        // Log all detected protocols with names
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            const char* proto_name = "Unknown";
            switch(event.data.protocols[i]) {
                case NfcProtocolIso14443_3a: proto_name = "ISO14443-3A"; break;
                case NfcProtocolIso14443_4a: proto_name = "ISO14443-4A (ISO-DEP)"; break;
                case NfcProtocolMfUltralight: proto_name = "MIFARE Ultralight"; break;
                case NfcProtocolIso15693_3: proto_name = "ISO15693"; break;
                default: proto_name = "Other"; break;
            }
            FURI_LOG_I(TAG, "  Protocol[%zu]: %d (%s)", i, event.data.protocols[i], proto_name);
        }

        // Check for protocols in priority order
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            NfcProtocol p = event.data.protocols[i];

            // Highest priority: MfUltralight (supports Type 2 NDEF)
            if(p == NfcProtocolMfUltralight) {
                protocol_to_use = p;
                FURI_LOG_I(TAG, "Using MF Ultralight protocol");
                break;
            }
            // Next: ISO14443-4A (supports Type 4 NDEF)
            if(p == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
            // Next: ISO15693 (supports Type 5 NDEF, UID always available)
            if(p == NfcProtocolIso15693_3 && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
            // Last: ISO14443-3A (UID only)
            if(p == NfcProtocolIso14443_3a && protocol_to_use == NfcProtocolInvalid) {
                protocol_to_use = p;
            }
        }

        // If no direct match, try parent protocols
        if(protocol_to_use == NfcProtocolInvalid && event.data.protocol_num > 0) {
            for(size_t i = 0; i < event.data.protocol_num; i++) {
                NfcProtocol p = event.data.protocols[i];
                NfcProtocol parent = nfc_protocol_get_parent(p);
                FURI_LOG_I(TAG, "  Protocol %d has parent: %d", p, parent);

                // Check parents in same priority order
                if(parent == NfcProtocolMfUltralight) {
                    protocol_to_use = parent;
                    FURI_LOG_I(TAG, "Using parent MF Ultralight");
                    break;
                }
                if(parent == NfcProtocolIso14443_4a && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
                if(parent == NfcProtocolIso15693_3 && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
                if(parent == NfcProtocolIso14443_3a && protocol_to_use == NfcProtocolInvalid) {
                    protocol_to_use = parent;
                }
            }
        }

        if(protocol_to_use != NfcProtocolInvalid) {
            const char* proto_name = "Unknown";
            switch(protocol_to_use) {
                case NfcProtocolIso14443_3a: proto_name = "ISO14443-3A"; break;
                case NfcProtocolIso14443_4a: proto_name = "ISO14443-4A (ISO-DEP)"; break;
                case NfcProtocolMfUltralight: proto_name = "MIFARE Ultralight"; break;
                case NfcProtocolIso15693_3: proto_name = "ISO15693"; break;
                default: proto_name = "Other"; break;
            }
            instance->detected_protocol = protocol_to_use;
            instance->state = FlipperWedgeNfcStateTagDetected;
            FURI_LOG_I(TAG, "*** SELECTED PROTOCOL: %d (%s) ***", protocol_to_use, proto_name);
        } else {
            FURI_LOG_W(TAG, "No supported protocol found");
        }
    }
}

// Internal function to switch from scanner to poller
static void flipper_wedge_nfc_start_poller(FlipperWedgeNfc* instance) {
    furi_assert(instance);

    // Stop and free scanner
    if(instance->scanner) {
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Start poller for the detected protocol
    instance->poller = nfc_poller_alloc(instance->nfc, instance->detected_protocol);
    if(instance->poller) {
        instance->state = FlipperWedgeNfcStatePolling;
        if(instance->detected_protocol == NfcProtocolMfUltralight) {
            nfc_poller_start(instance->poller, flipper_wedge_nfc_poller_callback_mf_ultralight, instance);
        } else if(instance->detected_protocol == NfcProtocolIso14443_3a) {
            nfc_poller_start(instance->poller, flipper_wedge_nfc_poller_callback_iso14443_3a, instance);
        } else if(instance->detected_protocol == NfcProtocolIso14443_4a) {
            nfc_poller_start(instance->poller, flipper_wedge_nfc_poller_callback_iso14443_4a, instance);
        } else if(instance->detected_protocol == NfcProtocolIso15693_3) {
            nfc_poller_start(instance->poller, flipper_wedge_nfc_poller_callback_iso15693, instance);
        }
        FURI_LOG_I(TAG, "Started poller for protocol %d", instance->detected_protocol);
    } else {
        FURI_LOG_E(TAG, "Failed to allocate poller");
        instance->state = FlipperWedgeNfcStateIdle;
    }
}

FlipperWedgeNfc* flipper_wedge_nfc_alloc(void) {
    FlipperWedgeNfc* instance = malloc(sizeof(FlipperWedgeNfc));

    instance->nfc = nfc_alloc();
    instance->scanner = NULL;
    instance->poller = NULL;
    instance->state = FlipperWedgeNfcStateIdle;
    instance->parse_ndef = false;
    instance->detected_protocol = NfcProtocolInvalid;
    instance->callback = NULL;
    instance->callback_context = NULL;
    instance->owner_thread = furi_thread_get_current_id();

    memset(&instance->last_data, 0, sizeof(FlipperWedgeNfcData));

    FURI_LOG_I(TAG, "NFC reader allocated");

    return instance;
}

void flipper_wedge_nfc_free(FlipperWedgeNfc* instance) {
    furi_assert(instance);

    flipper_wedge_nfc_stop(instance);

    if(instance->nfc) {
        nfc_free(instance->nfc);
        instance->nfc = NULL;
    }

    free(instance);
    FURI_LOG_I(TAG, "NFC reader freed");
}

void flipper_wedge_nfc_set_callback(
    FlipperWedgeNfc* instance,
    FlipperWedgeNfcCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

void flipper_wedge_nfc_start(FlipperWedgeNfc* instance, bool parse_ndef) {
    furi_assert(instance);

    FURI_LOG_I(TAG, "NFC start called, current state=%d, scanner=%p, poller=%p",
               instance->state, (void*)instance->scanner, (void*)instance->poller);

    if(instance->state != FlipperWedgeNfcStateIdle) {
        FURI_LOG_W(TAG, "Already scanning, state=%d", instance->state);
        return;
    }

    // Defensive cleanup - ensure no stale scanner/poller
    if(instance->poller) {
        FURI_LOG_W(TAG, "Stale poller found, cleaning up");
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }
    if(instance->scanner) {
        FURI_LOG_W(TAG, "Stale scanner found, cleaning up");
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    instance->parse_ndef = parse_ndef;
    instance->detected_protocol = NfcProtocolInvalid;
    memset(&instance->last_data, 0, sizeof(FlipperWedgeNfcData));

    // Create and start scanner
    instance->scanner = nfc_scanner_alloc(instance->nfc);
    if(!instance->scanner) {
        FURI_LOG_E(TAG, "Failed to allocate NFC scanner!");
        return;
    }

    nfc_scanner_start(instance->scanner, flipper_wedge_nfc_scanner_callback, instance);

    instance->state = FlipperWedgeNfcStateScanning;
    FURI_LOG_I(TAG, "NFC scanning started (NDEF: %s), scanner=%p", parse_ndef ? "ON" : "OFF", (void*)instance->scanner);
}

void flipper_wedge_nfc_stop(FlipperWedgeNfc* instance) {
    furi_assert(instance);

    FURI_LOG_I(TAG, "NFC stop called, state=%d", instance->state);

    if(instance->poller) {
        FURI_LOG_D(TAG, "Stopping poller");
        nfc_poller_stop(instance->poller);
        nfc_poller_free(instance->poller);
        instance->poller = NULL;
    }

    if(instance->scanner) {
        FURI_LOG_D(TAG, "Stopping scanner");
        nfc_scanner_stop(instance->scanner);
        nfc_scanner_free(instance->scanner);
        instance->scanner = NULL;
    }

    // Reset all state
    instance->state = FlipperWedgeNfcStateIdle;
    instance->detected_protocol = NfcProtocolInvalid;

    FURI_LOG_I(TAG, "NFC scanning stopped, state now Idle");
}

bool flipper_wedge_nfc_is_scanning(FlipperWedgeNfc* instance) {
    furi_assert(instance);
    return instance->state == FlipperWedgeNfcStateScanning ||
           instance->state == FlipperWedgeNfcStateTagDetected ||
           instance->state == FlipperWedgeNfcStatePolling ||
           instance->state == FlipperWedgeNfcStateError;  // Still scanning during error recovery
}

// Call this from the main thread's tick handler to process NFC events
// Returns true if a tag was successfully read (data available in last_data)
bool flipper_wedge_nfc_tick(FlipperWedgeNfc* instance) {
    furi_assert(instance);

    if(instance->state == FlipperWedgeNfcStateTagDetected) {
        // Scanner detected a tag, switch to poller (safe to do from main thread)
        FURI_LOG_I(TAG, "Tick: starting poller for detected tag, protocol=%d", instance->detected_protocol);
        flipper_wedge_nfc_start_poller(instance);
        return false;
    }

    if(instance->state == FlipperWedgeNfcStateError) {
        // Poller failed, recover by restarting scanner
        FURI_LOG_I(TAG, "Tick: poller error detected, recovering...");

        // Stop and free the failed poller
        if(instance->poller) {
            FURI_LOG_D(TAG, "Tick: stopping failed poller");
            nfc_poller_stop(instance->poller);
            nfc_poller_free(instance->poller);
            instance->poller = NULL;
        }

        // Restart the scanner
        FURI_LOG_I(TAG, "Tick: restarting scanner after error");
        instance->scanner = nfc_scanner_alloc(instance->nfc);
        nfc_scanner_start(instance->scanner, flipper_wedge_nfc_scanner_callback, instance);

        // Transition back to scanning state
        instance->state = FlipperWedgeNfcStateScanning;
        instance->detected_protocol = NfcProtocolInvalid;
        FURI_LOG_I(TAG, "Tick: error recovery complete, scanning resumed");
        return false;
    }

    if(instance->state == FlipperWedgeNfcStateSuccess) {
        // Poller got the UID, invoke callback
        FURI_LOG_I(TAG, "Tick: tag read success, UID len=%d, invoking callback", instance->last_data.uid_len);

        // Stop the poller first
        if(instance->poller) {
            FURI_LOG_D(TAG, "Tick: stopping poller");
            nfc_poller_stop(instance->poller);
            nfc_poller_free(instance->poller);
            instance->poller = NULL;
        }

        // Reset state to Idle BEFORE calling callback
        // This ensures the NFC module is ready for restart
        instance->state = FlipperWedgeNfcStateIdle;
        instance->detected_protocol = NfcProtocolInvalid;
        FURI_LOG_D(TAG, "Tick: state reset to Idle");

        // Call the callback from main thread (safe!)
        if(instance->callback) {
            FURI_LOG_D(TAG, "Tick: calling callback");
            instance->callback(&instance->last_data, instance->callback_context);
            FURI_LOG_D(TAG, "Tick: callback returned");
        }
        return true;
    }

    return false;
}
