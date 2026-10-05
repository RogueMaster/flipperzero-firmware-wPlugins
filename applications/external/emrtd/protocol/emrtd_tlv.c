/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The parser is written against a chip that may be lying. Every read is
 * bounded by the length the caller passed, lengths are checked against what is
 * left of the buffer before a value is ever pointed at, the indefinite form is
 * refused outright, and descent is capped, so neither a crafted file nor a
 * corrupted frame can walk off the buffer or exhaust the 8 KB stack the NFC
 * worker thread runs on.
 */

#include "emrtd_tlv.h"

/*
 * Iteration has three outcomes and the cursor has one field to record them
 * in, so a failed parse parks the offset on a value no buffer can reach.
 * emrtd_tlv_iter_exhausted() then reads as false, which is what tells a caller
 * that the file was malformed rather than merely finished.
 */
#define EMRTD_TLV_ITER_ERROR ((size_t) - 1)

/** A tag is packed into a uint32_t, so four bytes is the hard ceiling. */
#define EMRTD_TLV_MAX_TAG_BYTES 4

/**
 * Longest length field accepted.
 *
 * Four bytes describe a value of up to 4 GiB, which is already absurd for a
 * file that has to fit in a 186 KB heap; the limit is here to
 * keep the accumulator from overflowing, not to be generous.
 */
#define EMRTD_TLV_MAX_LENGTH_BYTES 4

/**
 * How deep emrtd_tlv_find_recursive() will descend.
 *
 * EF.SOD is the deepest structure a passport carries: ContentInfo, SignedData,
 * the certificate, its subject name and the attribute inside it come to about
 * ten levels. Sixteen leaves room without letting a crafted file turn nesting
 * into stack exhaustion.
 */
#define EMRTD_TLV_MAX_DEPTH 16

/**
 * Read a tag field.
 *
 * ISO/IEC 7816-4 section 5.2.2.1 rules out 00 and FF as the first byte of a
 * tag; 00 is padding and FF is reserved, and accepting either would let a run
 * of filler bytes at the end of a file parse as a node.
 */
static bool tlv_read_tag(
    const uint8_t* data,
    size_t len,
    size_t* pos,
    uint32_t* out_tag,
    bool* out_constructed) {
    if(*pos >= len) {
        return false;
    }

    const uint8_t first = data[*pos];
    if(first == 0x00 || first == 0xFF) {
        return false;
    }

    *out_constructed = (first & 0x20) != 0;
    uint32_t tag = first;
    size_t bytes = 1;
    (*pos)++;

    if((first & 0x1F) == 0x1F) {
        /* High tag number form: subsequent bytes carry seven bits each. */
        bool more = true;
        while(more) {
            if(*pos >= len || bytes >= EMRTD_TLV_MAX_TAG_BYTES) {
                return false;
            }
            const uint8_t byte = data[*pos];
            tag = (tag << 8) | byte;
            bytes++;
            (*pos)++;
            more = (byte & 0x80) != 0;
        }
    }

    *out_tag = tag;
    return true;
}

/**
 * Read a length field.
 *
 * The indefinite form (0x80) is rejected: it would make the end of a value
 * depend on finding an end-of-contents marker somewhere further on, which is
 * exactly the kind of unbounded search this parser exists to avoid. ICAO Doc
 * 9303-10 requires DER for the LDS, where the indefinite form is not allowed
 * in the first place.
 */
static bool tlv_read_length(const uint8_t* data, size_t len, size_t* pos, size_t* out_len) {
    if(*pos >= len) {
        return false;
    }

    const uint8_t first = data[*pos];
    (*pos)++;

    if(first < 0x80) {
        *out_len = first;
        return true;
    }

    /* 0xFF is reserved by ISO/IEC 8825-1; 0x80 is the indefinite form. */
    if(first == 0x80 || first == 0xFF) {
        return false;
    }

    const size_t count = (size_t)(first & 0x7F);
    if(count > EMRTD_TLV_MAX_LENGTH_BYTES || count > len - *pos) {
        return false;
    }

    size_t value = 0;
    for(size_t i = 0; i < count; i++) {
        value = (value << 8) | data[*pos];
        (*pos)++;
    }

    *out_len = value;
    return true;
}

/** Parse the node that starts at @p offset, staying inside @p len. */
static bool tlv_read_node(const uint8_t* data, size_t len, size_t offset, EmrtdTlv* out) {
    size_t pos = offset;
    uint32_t tag = 0;
    bool constructed = false;
    size_t value_len = 0;

    if(!tlv_read_tag(data, len, &pos, &tag, &constructed)) {
        return false;
    }
    if(!tlv_read_length(data, len, &pos, &value_len)) {
        return false;
    }
    /* pos <= len holds here, so the subtraction cannot wrap. */
    if(value_len > len - pos) {
        return false;
    }

    out->tag = tag;
    out->value = data + pos;
    out->value_len = value_len;
    out->constructed = constructed;
    out->start = data + offset;
    out->total_len = (pos - offset) + value_len;
    return true;
}

void emrtd_tlv_iter_init(EmrtdTlvIter* iter, const uint8_t* data, size_t len) {
    if(iter == NULL) {
        return;
    }
    const bool usable = data != NULL && len != EMRTD_TLV_ITER_ERROR;
    iter->data = usable ? data : NULL;
    iter->len = usable ? len : 0;
    iter->offset = 0;
}

void emrtd_tlv_iter_children(EmrtdTlvIter* iter, const EmrtdTlv* node) {
    if(iter == NULL) {
        return;
    }
    /*
     * A primitive node has no children rather than an error: real documents
     * put unparsable content under constructed tags (a biometric header, for
     * one), and a caller that asks anyway should get an empty sequence.
     */
    if(node == NULL || !node->constructed) {
        emrtd_tlv_iter_init(iter, NULL, 0);
        return;
    }
    emrtd_tlv_iter_init(iter, node->value, node->value_len);
}

bool emrtd_tlv_iter_next(EmrtdTlvIter* iter, EmrtdTlv* out) {
    if(iter == NULL || out == NULL) {
        return false;
    }
    /* Covers the end of the buffer and the error parking spot in one test. */
    if(iter->data == NULL || iter->offset >= iter->len) {
        return false;
    }

    EmrtdTlv node;
    if(!tlv_read_node(iter->data, iter->len, iter->offset, &node)) {
        iter->offset = EMRTD_TLV_ITER_ERROR;
        return false;
    }

    /* A node is at least a tag byte and a length byte, so this advances. */
    iter->offset += node.total_len;
    *out = node;
    return true;
}

bool emrtd_tlv_iter_exhausted(const EmrtdTlvIter* iter) {
    return iter != NULL && iter->offset == iter->len;
}

bool emrtd_tlv_parse_first(const uint8_t* data, size_t len, EmrtdTlv* out) {
    if(data == NULL || out == NULL || len == 0) {
        return false;
    }
    return tlv_read_node(data, len, 0, out);
}

bool emrtd_tlv_find(const uint8_t* data, size_t len, uint32_t tag, EmrtdTlv* out) {
    if(out == NULL) {
        return false;
    }

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_init(&iter, data, len);
    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag == tag) {
            *out = node;
            return true;
        }
    }
    return false;
}

static bool tlv_find_recursive(
    const uint8_t* data,
    size_t len,
    uint32_t tag,
    EmrtdTlv* out,
    unsigned depth) {
    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_init(&iter, data, len);

    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag == tag) {
            *out = node;
            return true;
        }
        /*
         * A constructed node whose content is not TLV is not an error - the
         * search simply moves on to the next sibling, which is what lets this
         * work on the biometric templates of DG2.
         */
        if(node.constructed && node.value_len > 0 && depth + 1 < EMRTD_TLV_MAX_DEPTH) {
            if(tlv_find_recursive(node.value, node.value_len, tag, out, depth + 1)) {
                return true;
            }
        }
    }
    return false;
}

bool emrtd_tlv_find_recursive(const uint8_t* data, size_t len, uint32_t tag, EmrtdTlv* out) {
    if(out == NULL) {
        return false;
    }
    return tlv_find_recursive(data, len, tag, out, 0);
}

size_t emrtd_tlv_total_length(const uint8_t* header, size_t len) {
    if(header == NULL || len == 0) {
        return 0;
    }

    size_t pos = 0;
    uint32_t tag = 0;
    bool constructed = false;
    size_t value_len = 0;

    if(!tlv_read_tag(header, len, &pos, &tag, &constructed)) {
        return 0;
    }
    if(!tlv_read_length(header, len, &pos, &value_len)) {
        return 0;
    }
    /* The value has not been fetched yet, so only the sum has to be sane. */
    if(value_len > (size_t)-1 - pos) {
        return 0;
    }
    return pos + value_len;
}

bool emrtd_tlv_read_uint(const EmrtdTlv* node, uint32_t* out) {
    if(node == NULL || out == NULL || node->value == NULL || node->value_len == 0) {
        return false;
    }

    /*
     * DER writes a leading zero byte in front of an integer whose top bit is
     * set, so a four byte value can arrive in five bytes. Skipping the zeroes
     * first accepts that encoding without widening the accumulator.
     */
    size_t offset = 0;
    while(offset < node->value_len && node->value[offset] == 0x00) {
        offset++;
    }
    if(node->value_len - offset > 4) {
        return false;
    }

    uint32_t value = 0;
    for(size_t i = offset; i < node->value_len; i++) {
        value = (value << 8) | node->value[i];
    }
    *out = value;
    return true;
}

const char* emrtd_tlv_tag_str(uint32_t tag, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return out;
    }

    size_t bytes = 1;
    if(tag > 0xFFFFFFu) {
        bytes = 4;
    } else if(tag > 0xFFFFu) {
        bytes = 3;
    } else if(tag > 0xFFu) {
        bytes = 2;
    }

    static const char digits[] = "0123456789ABCDEF";
    size_t pos = 0;
    for(size_t i = bytes; i > 0 && pos + 2 < out_size; i--) {
        const uint8_t byte = (uint8_t)(tag >> ((i - 1) * 8));
        out[pos++] = digits[byte >> 4];
        out[pos++] = digits[byte & 0x0F];
    }
    out[pos] = '\0';
    return out;
}

size_t emrtd_tlv_encoded_length_size(size_t length) {
    if(length < 0x80u) {
        return 1;
    }
    if(length <= 0xFFu) {
        return 2;
    }
    if(length <= 0xFFFFu) {
        return 3;
    }
    if(length <= 0xFFFFFFu) {
        return 4;
    }
    /* Beyond three length bytes nothing in an eMRTD needs encoding. */
    return 0;
}

size_t emrtd_tlv_encode_length(size_t length, uint8_t* out) {
    if(out == NULL) {
        return 0;
    }

    const size_t size = emrtd_tlv_encoded_length_size(length);
    switch(size) {
    case 1:
        out[0] = (uint8_t)length;
        break;
    case 2:
        out[0] = 0x81;
        out[1] = (uint8_t)length;
        break;
    case 3:
        out[0] = 0x82;
        out[1] = (uint8_t)(length >> 8);
        out[2] = (uint8_t)length;
        break;
    case 4:
        out[0] = 0x83;
        out[1] = (uint8_t)(length >> 16);
        out[2] = (uint8_t)(length >> 8);
        out[3] = (uint8_t)length;
        break;
    default:
        return 0;
    }
    return size;
}
