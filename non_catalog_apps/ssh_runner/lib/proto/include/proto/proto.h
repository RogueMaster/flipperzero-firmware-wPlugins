/*
 * SSH Runner UART protocol — framing layer.
 *
 * Portable C99, no allocation. Shared by the Flipper app, the ESP32 firmware
 * and the host unit tests. The spec is docs/PROTOCOL.md; keep both in sync.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_VERSION 1

/* Frame layout before COBS: type:u8 | seq:u16le | len:u16le | payload | crc16:u16le */
#define PROTO_MAX_PAYLOAD 512u
#define PROTO_HDR_LEN     5u
#define PROTO_CRC_LEN     2u
#define PROTO_MAX_RAW     (PROTO_HDR_LEN + PROTO_MAX_PAYLOAD + PROTO_CRC_LEN)
/* COBS adds at most one byte per 254 bytes, plus one. */
#define PROTO_COBS_MAX(n) ((n) + ((n) / 254u) + 1u)
/* Encoded frame on the wire: 0x00 | cobs(raw) | 0x00 */
#define PROTO_MAX_WIRE    (PROTO_COBS_MAX(PROTO_MAX_RAW) + 2u)

#define PROTO_BAUD_INITIAL 115200u
#define PROTO_BAUD_MAX     921600u

/* Bit 7 of the type marks a reply (ACK, ERROR or typed response). */
#define PROTO_REPLY_BIT   0x80u
#define PROTO_IS_REPLY(t) (((t) & PROTO_REPLY_BIT) != 0u)

/* ---- CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection) ---- */
#define PROTO_CRC16_INIT 0xFFFFu
uint16_t proto_crc16(const uint8_t* data, size_t len, uint16_t crc);

/* ---- COBS ----
 * Both return the output length, or 0 on error (output too small / malformed).
 * Decoding may be done in place (out == in). */
size_t proto_cobs_encode(const uint8_t* in, size_t len, uint8_t* out, size_t out_cap);
size_t proto_cobs_decode(const uint8_t* in, size_t len, uint8_t* out, size_t out_cap);

/* ---- Frames ---- */
typedef struct {
    uint8_t type;
    uint16_t seq;
    uint16_t len;
    const uint8_t* payload; /* points into the decoder buffer; valid until the next push */
} ProtoFrame;

/* Encode a whole frame including both 0x00 delimiters.
 * Returns the number of bytes written, or 0 if len > PROTO_MAX_PAYLOAD or out_cap is too small. */
size_t proto_frame_encode(
    uint8_t type,
    uint16_t seq,
    const uint8_t* payload,
    uint16_t len,
    uint8_t* out,
    size_t out_cap);

typedef enum {
    ProtoDecNone = 0, /* need more bytes */
    ProtoDecFrame, /* *out holds a valid frame */
    ProtoDecErrCobs, /* malformed COBS; frame dropped */
    ProtoDecErrLen, /* header length disagrees with frame size; dropped */
    ProtoDecErrCrc, /* CRC mismatch; dropped */
    ProtoDecErrOverflow, /* frame longer than PROTO_MAX_WIRE; dropped until next 0x00 */
} ProtoDecResult;

typedef struct {
    uint32_t frames;
    uint32_t err_cobs;
    uint32_t err_len;
    uint32_t err_crc;
    uint32_t err_overflow;
} ProtoStats;

typedef struct {
    uint8_t buf[PROTO_MAX_WIRE];
    size_t n;
    bool overflow;
    ProtoStats stats;
} ProtoDecoder;

void proto_decoder_init(ProtoDecoder* dec);
/* Feed one byte. Errors are reported once per bad frame; the decoder resyncs on
 * the next 0x00 by itself. Empty frames (back-to-back delimiters) are ignored. */
ProtoDecResult proto_decoder_push(ProtoDecoder* dec, uint8_t byte, ProtoFrame* out);

#ifdef __cplusplus
}
#endif
