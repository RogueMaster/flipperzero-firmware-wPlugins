#include "openprinttag_i.h"
#include <furi.h>
#include <string.h>

// NDEF Record structure
#define NDEF_MB       0x80 // Message Begin
#define NDEF_ME       0x40 // Message End
#define NDEF_CF       0x20 // Chunk Flag
#define NDEF_SR       0x10 // Short Record
#define NDEF_IL       0x08 // ID Length present
#define NDEF_TNF_MASK 0x07 // Type Name Format

#define TNF_MEDIA_TYPE 0x02

bool openprinttag_parse_ndef(OpenPrintTag* app, const uint8_t* data, size_t size) {
    if(size < 3) {
        FURI_LOG_E(TAG, "NDEF data too short");
        return false;
    }

    size_t offset = 0;

    // NFC Forum Type 5 tags start with a Capability Container:
    // 0xE1 = 4 bytes, 0xE2 = 8 bytes
    if(data[0] == 0xE1) {
        offset = 4;
    } else if(data[0] == 0xE2) {
        offset = 8;
    }

    // The NDEF message is wrapped in a TLV (Type-Length-Value). Walk the TLVs after the
    // Capability Container until the NDEF one (0x03) is found.
    if(offset != 0 || data[0] == 0x03) {
        bool found_ndef_tlv = false;

        while(offset < size) {
            uint8_t tlv_type = data[offset];

            if(tlv_type == 0x00) { // NULL TLV, single byte padding
                offset++;
                continue;
            }
            if(tlv_type == 0xFE) break; // Terminator TLV, nothing after it

            if(offset + 2 > size) return false;
            uint32_t length = data[offset + 1];
            size_t header_size = 2;

            // Handle 3-byte length format
            if(length == 0xFF) {
                if(offset + 4 > size) return false;
                length = (data[offset + 2] << 8) | data[offset + 3];
                header_size = 4;
            }

            if(tlv_type == 0x03) { // NDEF Message TLV
                FURI_LOG_D(TAG, "NDEF TLV at %zu, length: %lu", offset, length);
                offset += header_size;
                found_ndef_tlv = true;
                break;
            }

            // Skip other TLVs (lock control, memory control, ...)
            offset += header_size + length;
        }

        if(!found_ndef_tlv) {
            FURI_LOG_E(TAG, "No NDEF TLV found");
            return false;
        }
    }

    // An NDEF message can hold several records (for example a URL record next to the
    // OpenPrintTag one), so walk them until the OpenPrintTag media record is found
    uint32_t payload_length = 0;
    bool found_record = false;

    while(!found_record && offset < size) {
        // A zero flags byte cannot start a record (the first one needs the MB flag), so this is
        // erased or unwritten memory
        if(data[offset] == 0x00) {
            FURI_LOG_E(TAG, "No NDEF records at offset %zu (empty tag?)", offset);
            return false;
        }

        // Parse NDEF record header
        uint8_t flags = data[offset++];
        uint8_t tnf = flags & NDEF_TNF_MASK;
        bool short_record = (flags & NDEF_SR) != 0;
        bool id_length_present = (flags & NDEF_IL) != 0;

        FURI_LOG_D(TAG, "NDEF flags: 0x%02X, TNF: %d", flags, tnf);

        if(offset >= size) return false;
        uint8_t type_length = data[offset++];

        // Payload length
        if(offset >= size) return false;
        if(short_record) {
            payload_length = data[offset++];
        } else {
            if(size - offset < 4) return false;
            payload_length = ((uint32_t)data[offset] << 24) | ((uint32_t)data[offset + 1] << 16) |
                             ((uint32_t)data[offset + 2] << 8) | data[offset + 3];
            offset += 4;
        }

        // ID length (if present)
        uint8_t id_length = 0;
        if(id_length_present) {
            if(offset >= size) return false;
            id_length = data[offset++];
        }

        // Type field
        if(size - offset < type_length) return false;
        const char* type = (const char*)&data[offset];
        offset += type_length;

        // Skip ID if present
        if(size - offset < id_length) return false;
        offset += id_length;

        // Payload
        if(payload_length > size - offset) {
            FURI_LOG_E(TAG, "Invalid payload length");
            return false;
        }

        FURI_LOG_D(
            TAG,
            "Record: TNF %d, type length %d, payload length %lu",
            tnf,
            type_length,
            payload_length);

        // Check if it is the OpenPrintTag media record
        if(tnf == TNF_MEDIA_TYPE && type_length == strlen(OPENPRINTTAG_MIME_TYPE) &&
           memcmp(type, OPENPRINTTAG_MIME_TYPE, type_length) == 0) {
            found_record = true;
            break;
        }

        // Not ours, move on to the next record
        offset += payload_length;
        if(flags & NDEF_ME) break;
    }

    if(!found_record) {
        FURI_LOG_E(TAG, "No OpenPrintTag record in NDEF message");
        return false;
    }

    const uint8_t* payload = &data[offset];
    app->tag_data.ndef_payload_offset = offset;

    // Store raw data
    if(app->tag_data.raw_data) {
        free(app->tag_data.raw_data);
    }
    app->tag_data.raw_data = malloc(payload_length);
    memcpy(app->tag_data.raw_data, payload, payload_length);
    app->tag_data.raw_data_size = payload_length;

    // Parse CBOR payload
    return openprinttag_parse_cbor(app, payload, payload_length);
}
