#include "snmp_ber_view.h"

void seader_ber_cursor_init(SeaderBerCursor* cursor, const uint8_t* buffer, size_t len) {
    if(!cursor) return;
    cursor->buffer = buffer;
    cursor->len = len;
    cursor->offset = 0U;
}

static bool seader_ber_read_length(SeaderBerCursor* cursor, size_t* value_len) {
    if(!cursor || !value_len || cursor->offset >= cursor->len) return false;

    uint8_t descriptor = cursor->buffer[cursor->offset++];
    if((descriptor & 0x80U) == 0U) {
        *value_len = descriptor;
        return true;
    }

    size_t length_len = descriptor & 0x7FU;
    if(length_len == 0U || length_len > sizeof(size_t)) return false;
    if(cursor->offset + length_len > cursor->len) return false;

    size_t parsed = 0U;
    for(size_t i = 0U; i < length_len; i++) {
        parsed = (parsed << 8U) | cursor->buffer[cursor->offset++];
    }

    *value_len = parsed;
    return true;
}

bool seader_ber_next_tlv(SeaderBerCursor* cursor, SeaderBerTlvView* tlv) {
    if(!cursor || !tlv || !cursor->buffer) return false;
    if(cursor->offset + 2U > cursor->len) return false;

    tlv->tag = cursor->buffer[cursor->offset++];

    size_t value_len = 0U;
    if(!seader_ber_read_length(cursor, &value_len)) return false;
    if(cursor->offset + value_len > cursor->len) return false;

    tlv->value.ptr = cursor->buffer + cursor->offset;
    tlv->value.len = value_len;
    cursor->offset += value_len;
    return true;
}

bool seader_ber_parse_uint32(SeaderBytesView value, uint32_t* out) {
    if(!value.ptr || value.len == 0U || !out) return false;
    if(value.len > 1U && value.ptr[0] == 0x00U) {
        value.ptr++;
        value.len--;
    }
    if(value.len == 0U || value.len > 4U) return false;

    uint32_t parsed = 0U;
    for(size_t i = 0U; i < value.len; i++) {
        parsed = (parsed << 8U) | value.ptr[i];
    }

    *out = parsed;
    return true;
}
