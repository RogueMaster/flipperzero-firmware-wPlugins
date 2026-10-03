#pragma once

// --------------------------------------------------------------------------
// Tiny bounded builder for Flipper raw IR timing buffers.
//
// Protocol-agnostic and header-only: copied verbatim into every app, never
// edited during a port. Depends on nothing from the Flipper SDK, so the
// protocol modules that use it stay host-testable.
//
// Every write is bounds-checked. Once the buffer overflows the builder latches
// `overflow` and drops the rest, so a wrong PROTO_IR_MAX_TIMINGS shows up as a
// failed encode instead of memory corruption.
// --------------------------------------------------------------------------

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t* t;
    size_t n;
    size_t cap;
    bool overflow;
} IrBuild;

static inline IrBuild ir_build_init(uint32_t* buf, size_t cap) {
    IrBuild b = {buf, 0, cap, false};
    return b;
}

static inline void ir_push(IrBuild* b, uint32_t us) {
    if(b->n >= b->cap) {
        b->overflow = true;
        return;
    }
    b->t[b->n++] = us;
}

/// A mark/space pair.
static inline void ir_item(IrBuild* b, uint32_t mark, uint32_t space) {
    ir_push(b, mark);
    ir_push(b, space);
}

/// One pulse-distance bit: fixed mark, value carried by the space length.
static inline void
    ir_bit(IrBuild* b, bool one, uint32_t mark, uint32_t one_space, uint32_t zero_space) {
    ir_item(b, mark, one ? one_space : zero_space);
}

/// One byte, least significant bit first.
static inline void
    ir_byte_lsb(IrBuild* b, uint8_t v, uint32_t mark, uint32_t one_space, uint32_t zero_space) {
    for(uint8_t i = 0; i < 8; i++) {
        ir_bit(b, (v >> i) & 1, mark, one_space, zero_space);
    }
}

/// One byte, most significant bit first.
static inline void
    ir_byte_msb(IrBuild* b, uint8_t v, uint32_t mark, uint32_t one_space, uint32_t zero_space) {
    for(uint8_t i = 8; i-- > 0;) {
        ir_bit(b, (v >> i) & 1, mark, one_space, zero_space);
    }
}

/// A byte array, least significant bit first within each byte.
static inline void ir_bytes_lsb(
    IrBuild* b,
    const uint8_t* data,
    size_t len,
    uint32_t mark,
    uint32_t one_space,
    uint32_t zero_space) {
    for(size_t i = 0; i < len; i++) {
        ir_byte_lsb(b, data[i], mark, one_space, zero_space);
    }
}

/// A byte array, most significant bit first within each byte.
static inline void ir_bytes_msb(
    IrBuild* b,
    const uint8_t* data,
    size_t len,
    uint32_t mark,
    uint32_t one_space,
    uint32_t zero_space) {
    for(size_t i = 0; i < len; i++) {
        ir_byte_msb(b, data[i], mark, one_space, zero_space);
    }
}

/// `nbits` of an integer, most significant bit first.
static inline void ir_int_msb(
    IrBuild* b,
    uint64_t v,
    uint8_t nbits,
    uint32_t mark,
    uint32_t one_space,
    uint32_t zero_space) {
    for(uint8_t i = nbits; i-- > 0;) {
        ir_bit(b, (v >> i) & 1, mark, one_space, zero_space);
    }
}

/// Finish: trailing mark, then report the length. A raw Flipper signal must
/// end on a mark, so callers close with this rather than a gap.
static inline bool ir_build_finish(IrBuild* b, uint32_t final_mark, size_t* out_count) {
    ir_push(b, final_mark);
    if(b->overflow) return false;
    *out_count = b->n;
    return true;
}

/// Hex digest of a byte array for the Extra screen: "A1 82 00.." style.
static inline void ir_format_bytes(const uint8_t* data, size_t len, char* out, size_t out_len) {
    static const char hex[] = "0123456789ABCDEF";
    size_t w = 0;
    for(size_t i = 0; i < len && w + 3 < out_len; i++) {
        if(i) out[w++] = ' ';
        if(w + 2 >= out_len) break;
        out[w++] = hex[(data[i] >> 4) & 0xF];
        out[w++] = hex[data[i] & 0xF];
    }
    if(len && w + 2 < out_len) {
        out[w++] = '.';
        out[w++] = '.';
    }
    out[w < out_len ? w : out_len - 1] = '\0';
}
