#pragma once

// Manchester-ASK modem for the 125 kHz RFID link. Pure C, NO furi includes, so
// it round-trips on the host test harness (tools/modem_test.c). All timings and
// tolerances come from rfid_modem_config.h.
//
// Frame on the air: [preamble ones][sync 0x7E][len][packet bytes], bits
// LSB-first. The encoder is pull-based (the DMA refill asks for the next
// carrier-cycle pairs); the decoder is fed capture runs and returns a complete
// packet when a frame closes. No modem CRC — the engine's packet CRC16 and the
// final MD5 do the filtering.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "rfid_modem_config.h"

// Hard cap on a packet the modem will carry (the on-wire len is a single byte).
// The engine validates the real packet length; this only sizes the buffers.
#define RFID_MODEM_MAX_PACKET 255u

// Worst-case run count for one encoded frame: two half-bit runs per encoded bit
// (preamble + sync + len + packet), plus the trailing inter-frame-gap run.
#define RFID_MODEM_MAX_RUNS \
    (2u * (RFID_MODEM_PREAMBLE_BITS + 8u + 8u + RFID_MODEM_MAX_PACKET * 8u) + 2u)

// ===== Encoder (tag / sender) ===============================================

typedef struct {
    uint8_t run_cyc[RFID_MODEM_MAX_RUNS]; // level runs in carrier cycles, alternating High,Low,...
    size_t nruns;
    size_t idx; // next run pair to emit (steps by 2)
} RfidModemEnc;

// Load a packet (len bytes, 1..RFID_MODEM_MAX_PACKET) as the next frame to emit.
// Resets the encoder cursor. A len outside range yields an empty (idle) encoder.
void rfid_modem_enc_set_frame(RfidModemEnc* enc, const uint8_t* packet, size_t len);

// Pull the next (duration, pulse) pair, in carrier cycles: the load is on for
// `pulse` cycles then off for `duration - pulse` cycles. Returns false when the
// frame (including its inter-frame gap) is exhausted — the caller then emits idle
// carrier until the next frame is set.
bool rfid_modem_enc_next(RfidModemEnc* enc, uint32_t* duration, uint32_t* pulse);

// True while the current frame still has pairs to emit.
static inline bool rfid_modem_enc_busy(const RfidModemEnc* enc) {
    return enc->idx < enc->nruns;
}

// ===== Decoder (reader / receiver) ==========================================

typedef enum {
    RfidModemHunt = 0, // searching for preamble + sync
    RfidModemRead, // locked, reading len + packet bytes
} RfidModemDecState;

typedef struct {
    RfidModemDecState state;

    // HUNT: sliding window of the most recent half-bit levels (raw, pre-polarity).
    uint8_t win[2u * RFID_MODEM_PREAMBLE_MATCH_BITS + 16u]; // preamble-match + sync half-bits
    int win_len;

    // READ: half-bit counter since lock (even index = a bit value, odd = skipped),
    // polarity chosen at lock, and the bytes being assembled.
    int pol; // 0 = direct, 1 = inverted
    uint32_t hb_count;
    uint8_t cur_byte;
    int bit_in_byte;
    uint32_t byte_idx; // 0 = len byte, then 1..len = packet
    uint32_t need_bytes; // 1 + len once len is known (0 = len not read yet)
    uint8_t frame[1u + RFID_MODEM_MAX_PACKET]; // [len][packet]
} RfidModemDec;

// Reset the decoder to sync-hunt (call on init and on any teardown).
void rfid_modem_dec_reset(RfidModemDec* dec);

// Feed one demodulated level run (its constant `level` and `duration_us`). When a
// frame closes, writes the packet bytes into out[0..cap) and returns the packet
// length (>=1); otherwise returns 0. A run longer than the inter-frame gap, or a
// run that matches neither the half-bit nor full-bit window, resets the decoder.
size_t rfid_modem_dec_feed(
    RfidModemDec* dec,
    bool level,
    uint32_t duration_us,
    uint8_t* out,
    size_t out_cap);
