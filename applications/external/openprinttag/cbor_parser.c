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

// Counts the elements of an indefinite-length map (pairs) or array without moving the parser
static bool cbor_count_indefinite(const CborParser* parser, bool is_map, size_t* count) {
    CborParser scan = *parser;
    size_t n = 0;

    while(true) {
        if(cbor_parser_remaining(&scan) < 1) return false;
        if(scan.data[scan.offset] == CBOR_BREAK) break;

        if(!cbor_skip_value(&scan)) return false;
        if(is_map && !cbor_skip_value(&scan)) return false;
        n++;
    }

    *count = n;
    return true;
}

static float cbor_half_to_float(uint16_t half) {
    uint32_t sign = (uint32_t)(half & 0x8000) << 16;
    uint32_t exponent = (half >> 10) & 0x1F;
    uint32_t mantissa = half & 0x3FF;
    uint32_t bits;

    if(exponent == 0) {
        if(mantissa == 0) {
            bits = sign;
        } else {
            // Subnormal half: normalize it
            exponent = 127 - 15 + 1;
            while(!(mantissa & 0x400)) {
                mantissa <<= 1;
                exponent--;
            }
            mantissa &= 0x3FF;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if(exponent == 31) {
        bits = sign | 0x7F800000 | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }

    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

// Converts an IEEE double to float with bit operations (no double arithmetic needed)
static float cbor_double_to_float(uint64_t double_bits) {
    uint32_t sign = (uint32_t)(double_bits >> 63) << 31;
    int32_t exponent = (int32_t)((double_bits >> 52) & 0x7FF);
    uint32_t mantissa = (uint32_t)((double_bits & 0xFFFFFFFFFFFFFULL) >> 29);
    uint32_t bits;

    if(exponent == 0x7FF) {
        bits = sign | 0x7F800000 | mantissa;
    } else {
        int32_t float_exponent = exponent - 1023 + 127;
        if(float_exponent <= 0) {
            bits = sign; // Too small for a float, flush to zero
        } else if(float_exponent >= 255) {
            bits = sign | 0x7F800000; // Too large, infinity
        } else {
            bits = sign | ((uint32_t)float_exponent << 23) | mantissa;
        }
    }

    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

bool cbor_parse_value(CborParser* parser, CborValue* value) {
    if(cbor_parser_remaining(parser) < 1) return false;

    uint8_t initial_byte = parser->data[parser->offset++];
    uint8_t major_type = initial_byte >> 5;
    uint8_t additional_info = initial_byte & 0x1F;

    uint64_t length = 0;
    value->indefinite = false;

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
    case CBOR_TYPE_MAP:
        value->type = (major_type == CBOR_TYPE_MAP) ? CborValueTypeMap : CborValueTypeArray;
        if(additional_info == CBOR_INFO_INDEFINITE) {
            value->indefinite = true;
            return cbor_count_indefinite(parser, major_type == CBOR_TYPE_MAP, &value->value.count);
        }
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        value->value.count = (size_t)length;
        return true;

    case CBOR_TYPE_TAG:
        // Skip tag and parse the tagged value
        if(!cbor_read_uint(parser, additional_info, &length)) return false;
        return cbor_parse_value(parser, value);

    case CBOR_TYPE_FLOAT:
        switch(additional_info) {
        case 20: // false
        case 21: // true
        case 22: // null
        case 23: // undefined
            value->type = CborValueTypeSimple;
            value->value.u64 = additional_info;
            return true;
        case CBOR_INFO_UINT16: // half precision float
            value->type = CborValueTypeFloat;
            if(!cbor_read_uint(parser, additional_info, &length)) return false;
            value->value.f32 = cbor_half_to_float((uint16_t)length);
            return true;
        case CBOR_INFO_UINT32: { // single precision float
            value->type = CborValueTypeFloat;
            if(!cbor_read_uint(parser, additional_info, &length)) return false;
            uint32_t bits = (uint32_t)length;
            memcpy(&value->value.f32, &bits, sizeof(bits));
            return true;
        }
        case CBOR_INFO_UINT64: // double precision float
            value->type = CborValueTypeFloat;
            if(!cbor_read_uint(parser, additional_info, &length)) return false;
            value->value.f32 = cbor_double_to_float(length);
            return true;
        default: // Break byte or unsupported simple value
            value->type = CborValueTypeUnknown;
            return false;
        }

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

bool cbor_skip_contents(CborParser* parser, const CborValue* value) {
    size_t elements;

    switch(value->type) {
    case CborValueTypeArray:
        elements = value->value.count;
        break;
    case CborValueTypeMap:
        elements = value->value.count * 2;
        break;
    default:
        return true;
    }

    for(size_t i = 0; i < elements; i++) {
        if(!cbor_skip_value(parser)) return false;
    }

    if(value->indefinite) {
        // Consume the break byte that closes the container
        if(cbor_parser_remaining(parser) < 1 || parser->data[parser->offset] != CBOR_BREAK) {
            return false;
        }
        parser->offset++;
    }

    return true;
}

bool cbor_skip_value(CborParser* parser) {
    CborValue value;
    if(!cbor_parse_value(parser, &value)) return false;

    return cbor_skip_contents(parser, &value);
}
