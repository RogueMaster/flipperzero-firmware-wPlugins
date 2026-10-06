#include "openprinttag_i.h"
#include "cbor_encoder.h"
#include "cbor_parser.h"
#include "openprinttag_fields.h"
#include <furi.h>

#define CBOR_MAP_INDEFINITE (0xBFU)

// Updating the consumed weight must not change any other field of the auxiliary section: fields
// with keys this app does not know (storage location, purchase data, vendor keys, ...) are kept
// as they are. So the existing section is walked pair by pair and every pair except the consumed
// weight is copied verbatim, then the consumed weight is added. The section is written as an
// indefinite-length map, which is what the specification recommends for it.
size_t openprinttag_encode_auxiliary(
    OpenPrintTag* app,
    uint8_t* buffer,
    size_t buffer_size,
    uint32_t consumed_weight) {
    if(buffer_size < 2) return 0;

    // The auxiliary section as it is on the tag now, if it was read
    const OpenPrintTagMeta* meta = &app->tag_data.meta;
    const uint8_t* old_data = NULL;
    size_t old_size = 0;
    if(app->tag_data.raw_data && meta->aux_region_offset > 0 &&
       meta->aux_region_offset < app->tag_data.raw_data_size) {
        old_data = app->tag_data.raw_data + meta->aux_region_offset;
        old_size = app->tag_data.raw_data_size - meta->aux_region_offset;
        if(meta->aux_region_size > 0 && meta->aux_region_size < old_size) {
            old_size = meta->aux_region_size;
        }
    }

    size_t pos = 0;
    buffer[pos++] = CBOR_MAP_INDEFINITE;
    bool had_consumed_weight = false;

    // A section that cannot be parsed (never written, or left over from something else) is
    // treated as empty. Pairs are copied up to the first one that cannot be parsed.
    CborParser parser;
    size_t count = 0;
    cbor_parser_init(&parser, old_data, old_size);
    if(old_data && cbor_parse_map(&parser, &count)) {
        for(size_t i = 0; i < count; i++) {
            const size_t pair_start = parser.offset;

            CborValue key;
            if(!cbor_parse_value(&parser, &key) || !cbor_skip_contents(&parser, &key)) break;
            if(!cbor_skip_value(&parser)) break;
            const size_t pair_size = parser.offset - pair_start;

            if(key.type == CborValueTypeUnsigned && key.value.u64 == AUX_CONSUMED_WEIGHT) {
                had_consumed_weight = true;
                continue;
            }

            // Leave room for the break byte
            if(buffer_size - pos < pair_size + 1) return 0;
            memcpy(&buffer[pos], &old_data[pair_start], pair_size);
            pos += pair_size;
        }
    }

    // Do not add a consumed weight of 0 to a section that did not have one
    if(consumed_weight > 0 || had_consumed_weight) {
        if(buffer_size - pos < 1) return 0;

        CborEncoder encoder;
        cbor_encoder_init(&encoder, &buffer[pos], buffer_size - pos - 1);
        if(!cbor_encode_uint(&encoder, AUX_CONSUMED_WEIGHT)) return 0;
        if(!cbor_encode_uint(&encoder, consumed_weight)) return 0;
        pos += cbor_encoder_get_size(&encoder);
    }

    if(buffer_size - pos < 1) return 0;
    buffer[pos++] = CBOR_BREAK;
    return pos;
}
