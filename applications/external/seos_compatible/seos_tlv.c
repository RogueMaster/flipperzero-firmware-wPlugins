#include "seos_tlv.h"

/* A tag whose low five bits are all set continues into further octets; an
 * octet with its top bit clear ends the run. Two octets maximum. */
#define TAG_CONTINUES_MASK 0x1f
#define TAG_MORE_OCTETS    0x80

/* A length octet below 0x80 is the length itself. 0x80 is the indefinite
 * form, which requires an end-of-contents marker and is not supported. Above
 * that, the low bits give the number of length octets that follow. */
#define LENGTH_LONG_FORM   0x80
#define LENGTH_OCTETS_MASK 0x7f

/* Reads a tag at `offset`, reporting where it ends. */
static bool read_tag(const uint8_t* data, size_t len, size_t offset, uint16_t* tag, size_t* end) {
    if(offset >= len) return false;

    uint8_t first = data[offset];
    if((first & TAG_CONTINUES_MASK) != TAG_CONTINUES_MASK) {
        *tag = first;
        *end = offset + 1;
        return true;
    }

    if(offset + 1 >= len) return false;
    uint8_t second = data[offset + 1];
    /* A third octet would not fit in the reported tag. */
    if((second & TAG_MORE_OCTETS) != 0) return false;

    *tag = (uint16_t)((first << 8) | second);
    *end = offset + 2;
    return true;
}

/* Reads a length at `offset` and reports where the value starts.
 *
 * Non-minimal lengths are accepted. Not all encoders emit the shortest form,
 * and rejecting them would drop otherwise valid messages. */
static bool
    read_length(const uint8_t* data, size_t len, size_t offset, size_t* value_len, size_t* end) {
    if(offset >= len) return false;

    uint8_t first = data[offset];
    if(first < LENGTH_LONG_FORM) {
        *value_len = first;
        *end = offset + 1;
        return true;
    }

    size_t octets = first & LENGTH_OCTETS_MASK;
    if(octets == 0 || octets > 2) return false;
    if(offset + 1 + octets > len) return false;

    size_t value = 0;
    for(size_t i = 0; i < octets; i++) {
        value = (value << 8) | data[offset + 1 + i];
    }
    *value_len = value;
    *end = offset + 1 + octets;
    return true;
}

bool seos_tlv_read_tag(const uint8_t* data, size_t len, size_t* offset, uint16_t* tag) {
    size_t end = 0;
    if(!read_tag(data, len, *offset, tag, &end)) return false;
    *offset = end;
    return true;
}

void seos_tlv_cursor_init(SeosTlvCursor* cursor, const uint8_t* data, size_t len) {
    cursor->data = data;
    cursor->len = len;
    cursor->offset = 0;
}

bool seos_tlv_cursor_done(const SeosTlvCursor* cursor) {
    return cursor->offset >= cursor->len;
}

bool seos_tlv_read_at(const uint8_t* data, size_t len, size_t offset, SeosTlvObject* out) {
    uint16_t tag = 0;
    size_t after_tag = 0;
    if(!read_tag(data, len, offset, &tag, &after_tag)) return false;

    size_t value_len = 0;
    size_t value_offset = 0;
    if(!read_length(data, len, after_tag, &value_len, &value_offset)) return false;

    /* Subtraction rather than addition: value_offset + value_len could wrap
     * for a length near SIZE_MAX. */
    if(value_len > len - value_offset) return false;

    out->tag = tag;
    out->value = data + value_offset;
    out->value_len = value_len;
    out->header_offset = offset;
    out->value_offset = value_offset;
    return true;
}

bool seos_tlv_read(SeosTlvCursor* cursor, SeosTlvObject* out) {
    if(!seos_tlv_read_at(cursor->data, cursor->len, cursor->offset, out)) return false;

    cursor->offset = out->value_offset + out->value_len;
    return true;
}

size_t seos_tlv_length_size(size_t value_len) {
    if(value_len < LENGTH_LONG_FORM) return 1;
    if(value_len <= 0xff) return 2;
    return 3;
}

size_t seos_tlv_write_header(uint8_t* out, uint16_t tag, size_t value_len) {
    size_t written = 0;

    if(tag > 0xff) {
        out[written++] = (uint8_t)(tag >> 8);
    }
    out[written++] = (uint8_t)(tag & 0xff);

    if(value_len < LENGTH_LONG_FORM) {
        out[written++] = (uint8_t)value_len;
    } else if(value_len <= 0xff) {
        out[written++] = LENGTH_LONG_FORM | 1;
        out[written++] = (uint8_t)value_len;
    } else {
        out[written++] = LENGTH_LONG_FORM | 2;
        out[written++] = (uint8_t)(value_len >> 8);
        out[written++] = (uint8_t)(value_len & 0xff);
    }

    return written;
}

void seos_tlv_append(BitBuffer* out, uint16_t tag, const uint8_t* value, size_t value_len) {
    uint8_t header[SEOS_TLV_HEADER_MAX];
    size_t header_len = seos_tlv_write_header(header, tag, value_len);

    bit_buffer_append_bytes(out, header, header_len);
    if(value_len > 0) bit_buffer_append_bytes(out, value, value_len);
}
