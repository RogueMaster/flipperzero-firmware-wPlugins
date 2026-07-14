#include "cbor_parser.h"
#include <string.h>

void cbor_parser_init(CborParser* parser, const uint8_t* data, size_t size) {
    parser->data = data;
    parser->size = size;
    parser->offset = 0;
}

size_t cbor_parser_remaining(const CborParser* parser) {
    if(parser->offset >= parser->size) return 0;
    return parser->size - parser->offset;
}

static bool cbor_read_uint(CborParser* parser, uint8_t additional_info, uint64_t* result) {
    if(additional_info < 24) {
        *result = additional_info;
        return true;
    }

    size_t bytes_needed = 0;
    switch(additional_info) {
    case CBOR_INFO_UINT8:
        bytes_needed = 1;
        break;
    case CBOR_INFO_UINT16:
        bytes_needed = 2;
        break;
    case CBOR_INFO_UINT32:
        bytes_needed = 4;
        break;
    case CBOR_INFO_UINT64:
        bytes_needed = 8;
        break;
    default:
        return false;
    }

    if(cbor_parser_remaining(parser) < bytes_needed) return false;

    *result = 0;
    for(size_t i = 0; i < bytes_needed; i++) {
        *result = (*result << 8) | parser->data[parser->offset++];
    }

    return true;
}

bool cbor_parse_value(CborParser* parser, CborValue* value) {
    if(cbor_parser_remaining(parser) < 1) return false;

    uint8_t initial_byte = parser->data[parser->offset++];
    uint8_t major_type = initial_byte >> 5;
    uint8_t additional_info = initial_byte & 0x1F;

    uint64_t length = 0;

    switch(major_type) {
    case CBOR_TYPE_UINT:
        value->type = CborValueTypeUnsigned;
        return cbor_read_uint(parser, additional_info, &value->value.u64);

    case CBOR_TYPE_NINT:
        value->type = CborValueTypeSigned;
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        value->value.i64 = -1 - (int64_t)length;
        return true;

    case CBOR_TYPE_BYTES:
        value->type = CborValueTypeBytes;
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        if(cbor_parser_remaining(parser) < length) return false;
        value->value.bytes.data = &parser->data[parser->offset];
        value->value.bytes.size = (size_t)length;
        parser->offset += length;
        return true;

    case CBOR_TYPE_TEXT:
        value->type = CborValueTypeText;
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        if(cbor_parser_remaining(parser) < length) return false;
        value->value.text.data = &parser->data[parser->offset];
        value->value.text.size = (size_t)length;
        parser->offset += length;
        return true;

    case CBOR_TYPE_ARRAY:
        value->type = CborValueTypeArray;
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        value->value.count = (size_t)length;
        return true;

    case CBOR_TYPE_MAP:
        value->type = CborValueTypeMap;
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        value->value.count = (size_t)length;
        return true;

    case CBOR_TYPE_TAG:
        // Skip tag and parse the tagged value
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        return cbor_parse_value(parser, value);

    default:
        value->type = CborValueTypeUnknown;
        return false;
    }
}

bool cbor_parse_map(CborParser* parser, size_t* count) {
    CborValue value;
    if(!cbor_parse_value(parser, &value)) return false;
    if(value.type != CborValueTypeMap) return false;
    *count = value.value.count;
    return true;
}

bool cbor_skip_value(CborParser* parser) {
    CborValue value;
    if(!cbor_parse_value(parser, &value)) return false;

    switch(value.type) {
    case CborValueTypeArray:
        for(size_t i = 0; i < value.value.count; i++) {
            if(!cbor_skip_value(parser)) return false;
        }
        break;
    case CborValueTypeMap:
        for(size_t i = 0; i < value.value.count * 2; i++) {
            if(!cbor_skip_value(parser)) return false;
        }
        break;
    default:
        break;
    }

    return true;
}
