/* SPDX-License-Identifier: GPL-3.0-only */
#include "proto/proto.h"

#include <string.h>

uint16_t proto_crc16(const uint8_t* data, size_t len, uint16_t crc) {
    for(size_t i = 0; i < len; i++) {
        crc = (uint16_t)(crc ^ ((unsigned)data[i] << 8));
        for(int b = 0; b < 8; b++) {
            crc = (crc & 0x8000u) ? (uint16_t)(((unsigned)crc << 1) ^ 0x1021u) :
                                    (uint16_t)((unsigned)crc << 1);
        }
    }
    return crc;
}

size_t proto_cobs_encode(const uint8_t* in, size_t len, uint8_t* out, size_t out_cap) {
    if(out_cap < PROTO_COBS_MAX(len)) return 0;
    size_t code_pos = 0;
    size_t o = 1;
    uint8_t code = 1;
    for(size_t i = 0; i < len; i++) {
        if(in[i] == 0) {
            out[code_pos] = code;
            code_pos = o++;
            code = 1;
        } else {
            out[o++] = in[i];
            code++;
            if(code == 0xFF) {
                out[code_pos] = code;
                code_pos = o++;
                code = 1;
            }
        }
    }
    out[code_pos] = code;
    return o;
}

size_t proto_cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t out_cap) {
    size_t i = 0;
    size_t o = 0;
    while(i < len) {
        uint8_t code = in[i++];
        if(code == 0) return 0;
        for(uint8_t k = 1; k < code; k++) {
            if(i >= len || o >= out_cap) return 0;
            uint8_t c = in[i++];
            if(c == 0) return 0;
            out[o++] = c;
        }
        if(code != 0xFF && i < len) {
            if(o >= out_cap) return 0;
            out[o++] = 0;
        }
    }
    return o;
}

size_t proto_frame_encode(
    uint8_t type,
    uint16_t seq,
    const uint8_t* payload,
    uint16_t len,
    uint8_t* out,
    size_t out_cap) {
    if(len > PROTO_MAX_PAYLOAD) return 0;
    uint8_t raw[PROTO_MAX_RAW];
    raw[0] = type;
    raw[1] = (uint8_t)(seq & 0xFF);
    raw[2] = (uint8_t)(seq >> 8);
    raw[3] = (uint8_t)(len & 0xFF);
    raw[4] = (uint8_t)(len >> 8);
    if(len) memcpy(&raw[PROTO_HDR_LEN], payload, len);
    size_t n = PROTO_HDR_LEN + len;
    uint16_t crc = proto_crc16(raw, n, PROTO_CRC16_INIT);
    raw[n++] = (uint8_t)(crc & 0xFF);
    raw[n++] = (uint8_t)(crc >> 8);

    if(out_cap < 2) return 0;
    out[0] = 0x00;
    size_t enc = proto_cobs_encode(raw, n, &out[1], out_cap - 2);
    if(enc == 0) return 0;
    out[1 + enc] = 0x00;
    return enc + 2;
}

void proto_decoder_init(ProtoDecoder* dec) {
    memset(dec, 0, sizeof(*dec));
}

ProtoDecResult proto_decoder_push(ProtoDecoder* dec, uint8_t byte, ProtoFrame* out) {
    if(byte != 0x00) {
        if(dec->overflow) return ProtoDecNone;
        if(dec->n >= sizeof(dec->buf)) {
            dec->overflow = true;
            dec->n = 0;
            dec->stats.err_overflow++;
            return ProtoDecErrOverflow;
        }
        dec->buf[dec->n++] = byte;
        return ProtoDecNone;
    }

    /* Delimiter: end of a frame. */
    size_t n = dec->n;
    dec->n = 0;
    if(dec->overflow) {
        dec->overflow = false;
        return ProtoDecNone;
    }
    if(n == 0) return ProtoDecNone;

    size_t raw_len = proto_cobs_decode(dec->buf, n, dec->buf, sizeof(dec->buf));
    if(raw_len == 0) {
        dec->stats.err_cobs++;
        return ProtoDecErrCobs;
    }
    if(raw_len < PROTO_HDR_LEN + PROTO_CRC_LEN) {
        dec->stats.err_len++;
        return ProtoDecErrLen;
    }
    uint16_t len = (uint16_t)(dec->buf[3] | ((uint16_t)dec->buf[4] << 8));
    if(len > PROTO_MAX_PAYLOAD || (size_t)len + PROTO_HDR_LEN + PROTO_CRC_LEN != raw_len) {
        dec->stats.err_len++;
        return ProtoDecErrLen;
    }
    uint16_t want = (uint16_t)(dec->buf[raw_len - 2] | ((uint16_t)dec->buf[raw_len - 1] << 8));
    if(proto_crc16(dec->buf, raw_len - 2, PROTO_CRC16_INIT) != want) {
        dec->stats.err_crc++;
        return ProtoDecErrCrc;
    }
    out->type = dec->buf[0];
    out->seq = (uint16_t)(dec->buf[1] | ((uint16_t)dec->buf[2] << 8));
    out->len = len;
    out->payload = &dec->buf[PROTO_HDR_LEN];
    dec->stats.frames++;
    return ProtoDecFrame;
}
