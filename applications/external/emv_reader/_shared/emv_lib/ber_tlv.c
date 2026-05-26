#include "ber_tlv.h"

size_t ber_tlv_read_tag(const uint8_t* buf, size_t len, uint32_t* out_tag) {
    if(!buf || len == 0) return 0;

    uint32_t tag = buf[0];
    size_t consumed = 1;

    if((buf[0] & 0x1F) == 0x1F) {
        do {
            if(consumed >= len) return 0;
            tag = (tag << 8) | buf[consumed];
            consumed++;
            if(consumed > 4) return 0;
        } while(buf[consumed - 1] & 0x80);
    }

    if(out_tag) *out_tag = tag;
    return consumed;
}

size_t ber_tlv_read_length(const uint8_t* buf, size_t len, size_t* out_length) {
    if(!buf || len == 0) return 0;

    uint8_t b0 = buf[0];
    if((b0 & 0x80) == 0) {
        if(out_length) *out_length = b0;
        return 1;
    }

    uint8_t n = b0 & 0x7F;
    if(n == 0 || n > 4) return 0;
    if(1 + (size_t)n > len) return 0;

    size_t length = 0;
    for(uint8_t i = 0; i < n; i++) {
        length = (length << 8) | buf[1 + i];
    }
    if(out_length) *out_length = length;
    return 1 + n;
}

static bool ber_tlv_is_constructed_tag(uint32_t tag) {
    uint8_t first;
    if(tag > 0xFFFFFF) first = (uint8_t)(tag >> 24);
    else if(tag > 0xFFFF) first = (uint8_t)(tag >> 16);
    else if(tag > 0xFF) first = (uint8_t)(tag >> 8);
    else first = (uint8_t)tag;
    return (first & 0x20) != 0;
}

static bool ber_tlv_walk_inner(
    const uint8_t* buf,
    size_t len,
    BerTlvCallback cb,
    void* ctx,
    int depth) {
    size_t pos = 0;
    while(pos < len) {
        while(pos < len && (buf[pos] == 0x00 || buf[pos] == 0xFF)) pos++;
        if(pos >= len) break;

        uint32_t tag = 0;
        size_t tag_len = ber_tlv_read_tag(buf + pos, len - pos, &tag);
        if(tag_len == 0) return false;
        pos += tag_len;

        size_t value_len = 0;
        size_t len_len = ber_tlv_read_length(buf + pos, len - pos, &value_len);
        if(len_len == 0) return false;
        pos += len_len;

        if(value_len > len - pos) return false;

        bool constructed = ber_tlv_is_constructed_tag(tag);

        BerTlvField field = {
            .tag = tag,
            .constructed = constructed,
            .value = buf + pos,
            .length = value_len,
            .depth = depth,
        };
        if(!cb(&field, ctx)) return false;

        if(constructed) {
            if(!ber_tlv_walk_inner(buf + pos, value_len, cb, ctx, depth + 1)) return false;
        }

        pos += value_len;
    }
    return true;
}

bool ber_tlv_walk(const uint8_t* buf, size_t len, BerTlvCallback cb, void* ctx) {
    if(!buf || !cb) return false;
    return ber_tlv_walk_inner(buf, len, cb, ctx, 0);
}

typedef struct {
    uint32_t target_tag;
    const uint8_t* value;
    size_t length;
    bool found;
} FindCtx;

static bool ber_tlv_find_cb(const BerTlvField* field, void* ctx) {
    FindCtx* fc = (FindCtx*)ctx;
    if(field->tag == fc->target_tag) {
        fc->value = field->value;
        fc->length = field->length;
        fc->found = true;
        return false;
    }
    return true;
}

bool ber_tlv_find(
    const uint8_t* buf,
    size_t len,
    uint32_t tag,
    const uint8_t** out_value,
    size_t* out_len) {
    FindCtx fc = {.target_tag = tag, .found = false};
    ber_tlv_walk(buf, len, ber_tlv_find_cb, &fc);
    if(!fc.found) return false;
    if(out_value) *out_value = fc.value;
    if(out_len) *out_len = fc.length;
    return true;
}
