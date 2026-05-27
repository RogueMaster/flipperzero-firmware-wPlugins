/*
 * ZeroFIDO
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 or later.
 */

#include "ecdsa_der.h"

#include <string.h>

static uint8_t zf_der_encode_int(uint8_t *out, const uint8_t *value, size_t value_len) {
    size_t start = 0;
    while (start + 1U < value_len && value[start] == 0) {
        start++;
    }

    size_t len = value_len - start;
    out[0] = 0x02;
    if (value[start] & 0x80U) {
        out[1] = (uint8_t)(len + 1U);
        out[2] = 0;
        memcpy(&out[3], value + start, len);
        return (uint8_t)(len + 3U);
    }

    out[1] = (uint8_t)len;
    memcpy(&out[2], value + start, len);
    return (uint8_t)(len + 2U);
}

size_t zf_ecdsa_der_encode_signature(const uint8_t r[ZF_PUBLIC_KEY_LEN],
                                     const uint8_t s[ZF_PUBLIC_KEY_LEN], uint8_t *out,
                                     size_t capacity) {
    uint8_t tmp[80];
    uint8_t r_len = 0;
    uint8_t s_len = 0;
    size_t total = 0;

    if (!r || !s || !out) {
        return 0;
    }

    r_len = zf_der_encode_int(tmp, r, ZF_PUBLIC_KEY_LEN);
    s_len = zf_der_encode_int(tmp + r_len, s, ZF_PUBLIC_KEY_LEN);
    total = 2U + r_len + s_len;

    if (total > capacity) {
        return 0;
    }

    out[0] = 0x30;
    out[1] = (uint8_t)(r_len + s_len);
    memcpy(&out[2], tmp, r_len + s_len);
    return total;
}

static bool zf_der_decode_length(const uint8_t *input, size_t input_len, size_t *header_len,
                                 size_t *value_len) {
    if (!input || input_len < 2U || !header_len || !value_len) {
        return false;
    }

    if ((input[1] & 0x80U) == 0) {
        *header_len = 2;
        *value_len = input[1];
        return *value_len <= input_len - *header_len;
    }

    size_t length_octets = input[1] & 0x7FU;
    if (length_octets == 0 || length_octets > sizeof(size_t) || length_octets > input_len - 2U) {
        return false;
    }

    size_t length = 0;
    for (size_t i = 0; i < length_octets; ++i) {
        length = (length << 8) | input[2 + i];
    }

    *header_len = 2U + length_octets;
    *value_len = length;
    return *value_len <= input_len - *header_len;
}

static bool zf_der_decode_int_fixed(const uint8_t *input, size_t input_len, size_t *consumed,
                                    uint8_t out[ZF_PUBLIC_KEY_LEN]) {
    size_t header_len = 0;
    size_t value_len = 0;
    const uint8_t *value = NULL;

    if (!input || input_len < 2U || !consumed || !out || input[0] != 0x02) {
        return false;
    }
    if (!zf_der_decode_length(input, input_len, &header_len, &value_len) || value_len == 0 ||
        header_len > input_len || value_len > input_len - header_len) {
        return false;
    }

    *consumed = header_len + value_len;
    value = input + header_len;
    while (value_len > ZF_PUBLIC_KEY_LEN && *value == 0) {
        ++value;
        --value_len;
    }
    if (value_len > ZF_PUBLIC_KEY_LEN) {
        return false;
    }

    memset(out, 0, ZF_PUBLIC_KEY_LEN);
    memcpy(out + (ZF_PUBLIC_KEY_LEN - value_len), value, value_len);
    return true;
}

bool zf_ecdsa_der_decode_signature(const uint8_t *signature, size_t signature_len,
                                   uint8_t raw_signature[ZF_PUBLIC_KEY_LEN * 2U]) {
    size_t seq_header_len = 0;
    size_t seq_len = 0;
    size_t consumed = 0;
    size_t offset = 0;

    if (!signature || !raw_signature || signature_len < 2U || signature[0] != 0x30) {
        return false;
    }
    if (!zf_der_decode_length(signature, signature_len, &seq_header_len, &seq_len)) {
        return false;
    }

    offset = seq_header_len;
    if (!zf_der_decode_int_fixed(&signature[offset], signature_len - offset, &consumed,
                                 raw_signature)) {
        return false;
    }
    offset += consumed;
    if (!zf_der_decode_int_fixed(&signature[offset], signature_len - offset, &consumed,
                                 raw_signature + ZF_PUBLIC_KEY_LEN)) {
        return false;
    }
    offset += consumed;
    return offset == seq_header_len + seq_len;
}
