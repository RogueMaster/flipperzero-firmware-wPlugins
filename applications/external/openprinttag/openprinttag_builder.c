#include "openprinttag_i.h"
#include "cbor_encoder.h"
#include "material_types.h"
#include "openprinttag_fields.h"

#include <string.h>

// Layout of a new tag, modelled on tags made by the vendor:
//
//   capability container (4 bytes)   E1 40 <size/8> 01
//   NDEF TLV                         03 <length>
//     one NDEF media record          application/vnd.openprinttag
//       meta section                 {2: aux region offset}
//       main section                 the material data, an indefinite-length map
//       spare bytes                  room for the main section to grow, fills the NDEF area
//       auxiliary region             32 bytes at the end, {} now, holds the consumed weight
//   terminator TLV                   FE

#define CC_SIZE             (4U)
#define CC_MAGIC            (0xE1U)
#define CC_VERSION          (0x40U) // Version 1.0, read and write access
#define CC_FEATURES         (0x01U)
#define TLV_NDEF            (0x03U)
#define TLV_END             (0xFEU)
#define NDEF_MB_ME_SR_MEDIA (0xD2U) // Message begin + end, short record, media type
#define NDEF_MB_ME_MEDIA    (0xC2U) // Same with a 4 byte payload length

#define AUX_REGION_SIZE  (32U)
#define MAIN_BUFFER_SIZE (256U)

#define CBOR_MAP_INDEFINITE (0xBFU)
#define CBOR_BREAK          (0xFFU)
#define CBOR_MAP_1          (0xA1U) // Map with one pair

// Size of an unsigned integer in its shortest CBOR encoding. Strict decoders reject longer ones.
static size_t cbor_uint_size(size_t value) {
    if(value < 24) return 1;
    if(value <= 0xFF) return 2;
    return 3;
}

static size_t cbor_put_uint(uint8_t* out, size_t value) {
    if(value < 24) {
        out[0] = (uint8_t)value;
        return 1;
    }
    if(value <= 0xFF) {
        out[0] = 0x18;
        out[1] = (uint8_t)value;
        return 2;
    }
    out[0] = 0x19;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)value;
    return 3;
}

static bool encode_text_field(CborEncoder* encoder, uint32_t key, const char* text) {
    return cbor_encode_uint(encoder, key) && cbor_encode_text(encoder, text, strlen(text));
}

static bool encode_uint_field(CborEncoder* encoder, uint32_t key, uint32_t value) {
    return cbor_encode_uint(encoder, key) && cbor_encode_uint(encoder, value);
}

static bool encode_raw(CborEncoder* encoder, const uint8_t* bytes, size_t size) {
    if(encoder->capacity - encoder->offset < size) return false;
    memcpy(&encoder->buffer[encoder->offset], bytes, size);
    encoder->offset += size;
    return true;
}

// The diameter is a "number", stored as a float. 1.75 mm is exact as a half precision float,
// 2.85 mm needs a single precision one.
static bool encode_diameter_field(CborEncoder* encoder, uint8_t diameter_index) {
    static const uint8_t diameter_1_75[] = {0xF9, 0x3F, 0x00};
    static const uint8_t diameter_2_85[] = {0xFA, 0x40, 0x36, 0x66, 0x66};

    if(!cbor_encode_uint(encoder, MAIN_FILAMENT_DIAMETER)) return false;
    return diameter_index == 1 ? encode_raw(encoder, diameter_2_85, sizeof(diameter_2_85)) :
                                 encode_raw(encoder, diameter_1_75, sizeof(diameter_1_75));
}

// Encodes the main section as an indefinite-length map, the way the vendor's tags are written.
// Returns its size, 0 on failure. Keys are written in ascending order.
static size_t build_main_section(const OpenPrintTagCreateData* data, uint8_t* out, size_t size) {
    if(data->type_index >= MATERIAL_TYPES_COUNT) return 0;
    if(size < 3) return 0;

    const bool has_material = data->material[0] != '\0';
    const bool has_brand = data->brand[0] != '\0';

    bool has_uuid = false;
    for(size_t i = 0; i < sizeof(data->instance_uuid); i++) {
        has_uuid = has_uuid || data->instance_uuid[i] != 0;
    }

    // The map header takes the first byte and the break the last one
    out[0] = CBOR_MAP_INDEFINITE;
    CborEncoder encoder;
    cbor_encoder_init(&encoder, out + 1, size - 2);

    bool ok = true;
    if(has_uuid) {
        ok = ok && cbor_encode_uint(&encoder, MAIN_INSTANCE_UUID) &&
             cbor_encode_bytes(&encoder, data->instance_uuid, sizeof(data->instance_uuid));
    }
    ok = ok && encode_uint_field(&encoder, MAIN_MATERIAL_CLASS, 0); // 0 = FFF filament
    ok = ok &&
         encode_uint_field(&encoder, MAIN_MATERIAL_TYPE, material_types[data->type_index].key);
    if(has_material) ok = ok && encode_text_field(&encoder, MAIN_MATERIAL_NAME, data->material);
    if(has_brand) ok = ok && encode_text_field(&encoder, MAIN_BRAND_NAME, data->brand);
    if(data->weight > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_NOMINAL_NETTO_FULL_WEIGHT, data->weight);
        ok = ok && encode_uint_field(&encoder, MAIN_ACTUAL_NETTO_FULL_WEIGHT, data->weight);
    }
    if(data->empty_weight > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_EMPTY_CONTAINER_WEIGHT, data->empty_weight);
    }
    if(data->has_color) {
        ok = ok && cbor_encode_uint(&encoder, MAIN_PRIMARY_COLOR) &&
             cbor_encode_bytes(&encoder, data->color, sizeof(data->color));
    }
    ok = ok && encode_diameter_field(&encoder, data->diameter_index);
    if(data->nozzle_min > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_MIN_PRINT_TEMPERATURE, data->nozzle_min);
    }
    if(data->nozzle_max > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_MAX_PRINT_TEMPERATURE, data->nozzle_max);
    }
    if(data->bed_min > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_MIN_BED_TEMPERATURE, data->bed_min);
    }
    if(data->bed_max > 0) {
        ok = ok && encode_uint_field(&encoder, MAIN_MAX_BED_TEMPERATURE, data->bed_max);
    }
    ok = ok &&
         encode_text_field(
             &encoder, MAIN_MATERIAL_ABBREVIATION, material_types[data->type_index].abbreviation);

    if(!ok) return 0;

    const size_t entries_size = cbor_encoder_get_size(&encoder);
    out[1 + entries_size] = CBOR_BREAK;
    return entries_size + 2;
}

size_t openprinttag_build_tag_image(
    const OpenPrintTagCreateData* data,
    size_t capacity,
    size_t block_size,
    uint8_t* out) {
    furi_check(data);
    furi_check(out);
    if(block_size == 0) return 0;

    uint8_t main_section[MAIN_BUFFER_SIZE];
    const size_t main_size = build_main_section(data, main_section, sizeof(main_section));
    if(main_size == 0) return 0;

    // The capability container announces the NDEF area in units of 8 bytes, counted from the start
    // of the tag. Like the vendor's tags and the other apps' tags the message fills that area, so
    // the main section has room to grow and the auxiliary region sits at the very end.
    size_t area_units = (capacity - CC_SIZE) / 8;
    if(area_units > 0xFF) area_units = 0xFF;
    const size_t area_size = area_units * 8;
    if(area_units == 0 || area_size > capacity) return 0;

    const size_t type_size = strlen(OPENPRINTTAG_MIME_TYPE);

    // TLV header: a 1 byte length, or 0xFF followed by 2 bytes for messages of 255 bytes or more
    const size_t fixed_size = CC_SIZE + 1; // Capability container and terminator
    if(area_size < fixed_size + 2) return 0;
    const size_t tlv_header_size = (area_size - fixed_size - 2 >= 0xFF) ? 4 : 2;
    const size_t message_size = area_size - fixed_size - tlv_header_size;

    // NDEF record header: a short record carries a 1 byte payload length, otherwise 4 bytes
    if(message_size < 6 + type_size) return 0;
    const bool short_record = message_size - 3 - type_size <= 0xFF;
    const size_t record_header_size = short_record ? 3 : 6;
    const size_t payload_size = message_size - record_header_size - type_size;

    // Payload: meta, main section, spare bytes, auxiliary region. The auxiliary region runs to
    // the end of the payload and starts on a block boundary of the tag (the specification
    // requires that), so it can be rewritten without touching the blocks before it.
    if(payload_size < AUX_REGION_SIZE) return 0;
    const size_t payload_start = CC_SIZE + tlv_header_size + record_header_size + type_size;
    const size_t aux_start =
        (payload_start + payload_size - AUX_REGION_SIZE) / block_size * block_size;
    if(aux_start < payload_start) return 0;
    const size_t aux_offset = aux_start - payload_start;
    const size_t aux_size = payload_size - aux_offset;
    const size_t meta_size = 2 + cbor_uint_size(aux_offset);
    if(meta_size + main_size > aux_offset) return 0;
    if(message_size > 0xFFFF) return 0;

    memset(out, 0, capacity);
    size_t pos = 0;

    out[pos++] = CC_MAGIC;
    out[pos++] = CC_VERSION;
    out[pos++] = (uint8_t)area_units;
    out[pos++] = CC_FEATURES;

    out[pos++] = TLV_NDEF;
    if(tlv_header_size == 2) {
        out[pos++] = (uint8_t)message_size;
    } else {
        out[pos++] = 0xFF;
        out[pos++] = (uint8_t)(message_size >> 8);
        out[pos++] = (uint8_t)message_size;
    }

    if(short_record) {
        out[pos++] = NDEF_MB_ME_SR_MEDIA;
        out[pos++] = (uint8_t)type_size;
        out[pos++] = (uint8_t)payload_size;
    } else {
        out[pos++] = NDEF_MB_ME_MEDIA;
        out[pos++] = (uint8_t)type_size;
        out[pos++] = (uint8_t)(payload_size >> 24);
        out[pos++] = (uint8_t)(payload_size >> 16);
        out[pos++] = (uint8_t)(payload_size >> 8);
        out[pos++] = (uint8_t)payload_size;
    }
    memcpy(&out[pos], OPENPRINTTAG_MIME_TYPE, type_size);
    pos += type_size;

    // Meta section: {2: aux offset}. Like the vendor's tags it has no aux size, the auxiliary
    // region runs to the end of the payload.
    out[pos++] = CBOR_MAP_1;
    out[pos++] = META_AUX_REGION_OFFSET;
    pos += cbor_put_uint(&out[pos], aux_offset);

    memcpy(&out[pos], main_section, main_size);
    pos += main_size;

    pos = aux_start; // The bytes in between are spare, left zero
    out[pos] = CBOR_MAP_INDEFINITE; // Empty auxiliary map, the rest of the region stays zero
    out[pos + 1] = CBOR_BREAK;
    pos += aux_size;

    out[pos++] = TLV_END;

    furi_check(pos == area_size);
    return area_size;
}
