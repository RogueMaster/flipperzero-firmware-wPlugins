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

    // Check if it starts with NDEF TLV (Type-Length-Value)
    // For NFC Forum tags, NDEF message is wrapped in TLV
    if(data[0] == 0x03) { // NDEF Message TLV
        offset = 1;
        uint32_t length = data[offset++];

        // Handle 3-byte length format
        if(length == 0xFF) {
            if(offset + 2 > size) return false;
            length = (data[offset] << 8) | data[offset + 1];
            offset += 2;
        }

        FURI_LOG_D(TAG, "NDEF TLV length: %lu", length);
    }

    if(offset >= size) return false;

    // Parse NDEF record header
    uint8_t flags = data[offset++];
    uint8_t tnf = flags & NDEF_TNF_MASK;
    bool short_record = (flags & NDEF_SR) != 0;
    bool id_length_present = (flags & NDEF_IL) != 0;

    FURI_LOG_D(TAG, "NDEF flags: 0x%02X, TNF: %d", flags, tnf);

    if(tnf != TNF_MEDIA_TYPE) {
        FURI_LOG_E(TAG, "Not a media type record (TNF: %d)", tnf);
        return false;
    }

    if(offset >= size) return false;
    uint8_t type_length = data[offset++];

    // Payload length
    if(offset >= size) return false;
    uint32_t payload_length;
    if(short_record) {
        payload_length = data[offset++];
    } else {
        if(offset + 4 > size) return false;
        payload_length = (data[offset] << 24) | (data[offset + 1] << 16) |
                         (data[offset + 2] << 8) | data[offset + 3];
        offset += 4;
    }

    FURI_LOG_D(TAG, "Type length: %d, Payload length: %lu", type_length, payload_length);

    // ID length (if present)
    uint8_t id_length = 0;
    if(id_length_present) {
        if(offset >= size) return false;
        id_length = data[offset++];
    }

    // Type field
    if(offset + type_length > size) return false;
    const char* mime_type = (const char*)&data[offset];
    offset += type_length;

    FURI_LOG_D(TAG, "MIME type: %.*s", type_length, mime_type);

    // Check if it matches OpenPrintTag MIME type
    if(type_length != strlen(OPENPRINTTAG_MIME_TYPE) ||
       memcmp(mime_type, OPENPRINTTAG_MIME_TYPE, type_length) != 0) {
        FURI_LOG_E(TAG, "Not an OpenPrintTag record");
        return false;
    }

    // Skip ID if present
    offset += id_length;

    // Payload
    if(offset + payload_length > size) {
        FURI_LOG_E(TAG, "Invalid payload length");
        return false;
    }

    const uint8_t* payload = &data[offset];

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
