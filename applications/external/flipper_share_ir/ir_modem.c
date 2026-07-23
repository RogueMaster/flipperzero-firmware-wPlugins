#include "ir_modem.h"
#include <string.h>

// --- Compile-time sanity checks on the tunables ---------------------------

// A max-level data space must be shorter than the RX silence timeout, otherwise
// the longest legal space would be mistaken for the end of a frame.
_Static_assert(
    (IR_MODEM_SPACE_BASE_US + (IR_MODEM_LEVELS - 1u) * IR_MODEM_SPACE_STEP_US) <
        IR_MODEM_RX_TIMEOUT_US,
    "max data space must be shorter than RX timeout");

// The RX timeout (frame delimiter) must be shorter than the leading gap, so two
// back-to-back frames are always separated by a detectable silence.
_Static_assert(
    IR_MODEM_RX_TIMEOUT_US < IR_MODEM_LEAD_GAP_US,
    "RX timeout must be shorter than the inter-frame lead gap");

_Static_assert(
    IR_MODEM_BITS_PER_SYMBOL >= 1u && IR_MODEM_BITS_PER_SYMBOL <= 4u,
    "BITS_PER_SYMBOL must be in 1..4");

// --- Whitening ------------------------------------------------------------

#if IR_MODEM_WHITENING
// 9-bit Galois LFSR, feedback x^9 + x^5 + 1 (period 511 > any single packet's
// bit count, so no repetition within a frame). Self-inverse via XOR.
static void ir_modem_whiten(uint8_t* buf, size_t len) {
    uint16_t state = 0x1FFu; // nonzero seed
    for(size_t i = 0; i < len; ++i) {
        uint8_t ks = 0;
        for(int b = 0; b < 8; ++b) {
            uint16_t lsb = state & 1u;
            ks = (uint8_t)(ks | (lsb << b));
            state >>= 1;
            if(lsb) state ^= 0x110u; // taps at bit 8 and bit 4
        }
        buf[i] ^= ks;
    }
}
#else
static void ir_modem_whiten(uint8_t* buf, size_t len) {
    (void)buf;
    (void)len;
}
#endif

// --- Helpers --------------------------------------------------------------

static inline uint32_t ir_modem_abs_diff(uint32_t a, uint32_t b) {
    return (a > b) ? (a - b) : (b - a);
}

// Quantize a measured space duration to the nearest level in [0, LEVELS-1].
static inline uint32_t ir_modem_space_to_level(uint32_t duration_us) {
    int32_t rel = (int32_t)duration_us - (int32_t)IR_MODEM_SPACE_BASE_US;
    if(rel <= 0) return 0;
    int32_t level =
        (rel + (int32_t)(IR_MODEM_SPACE_STEP_US / 2)) / (int32_t)IR_MODEM_SPACE_STEP_US;
    if(level < 0) level = 0;
    if(level > (int32_t)(IR_MODEM_LEVELS - 1u)) level = (int32_t)(IR_MODEM_LEVELS - 1u);
    return (uint32_t)level;
}

// --- Encode ---------------------------------------------------------------

size_t ir_modem_encode_bound(size_t len) {
    if(len == 0 || len > IR_MODEM_MAX_FRAME_BYTES) return 0;
    const size_t total_bits = len * 8u;
    const size_t nsym = (total_bits + IR_MODEM_BITS_PER_SYMBOL - 1u) / IR_MODEM_BITS_PER_SYMBOL;
    return 1u /*lead*/ + 2u /*sync*/ + 2u * nsym + 1u /*end mark*/;
}

size_t ir_modem_encode(const uint8_t* data, size_t len, uint32_t* timings, size_t timings_cap) {
    if(!data || !timings) return 0;
    if(len == 0 || len > IR_MODEM_MAX_FRAME_BYTES) return 0;

    const size_t need = ir_modem_encode_bound(len);
    if(need == 0 || need > timings_cap) return 0;

    // Whiten a local copy so the caller's buffer is untouched.
    uint8_t buf[IR_MODEM_MAX_FRAME_BYTES];
    memcpy(buf, data, len);
    ir_modem_whiten(buf, len);

    const size_t total_bits = len * 8u;
    const size_t k = IR_MODEM_BITS_PER_SYMBOL;

    size_t idx = 0;
    timings[idx++] = IR_MODEM_LEAD_GAP_US; // [0] Space
    timings[idx++] = IR_MODEM_SYNC_MARK_US; // [1] Mark
    timings[idx++] = IR_MODEM_SYNC_SPACE_US; // [2] Space

    size_t bit = 0;
    while(bit < total_bits) {
        uint32_t val = 0;
        for(size_t b = 0; b < k; ++b) {
            uint32_t v = 0;
            if(bit < total_bits) {
                v = (buf[bit >> 3] >> (bit & 7u)) & 1u; // LSB-first
            }
            val |= v << b;
            ++bit;
        }
        timings[idx++] = IR_MODEM_MARK_US; // data mark
        timings[idx++] = IR_MODEM_SPACE_BASE_US + val * IR_MODEM_SPACE_STEP_US; // data space
    }

    timings[idx++] = IR_MODEM_END_MARK_US; // trailing mark closes the last space
    return idx;
}

// --- Decode ---------------------------------------------------------------

bool ir_modem_decode(
    const IrModemEvent* events,
    size_t n_events,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!events || !out || !out_len || out_cap == 0) return false;

    // Find the sync mark: first Mark whose duration is close to SYNC_MARK_US.
    size_t i = 0;
    for(; i < n_events; ++i) {
        if(events[i].level && ir_modem_abs_diff(events[i].duration, IR_MODEM_SYNC_MARK_US) <=
                                  IR_MODEM_SYNC_MARK_TOL_US) {
            break;
        }
    }
    if(i >= n_events) return false; // no sync -> not our frame

    ++i; // consume sync mark
    if(i < n_events && !events[i].level) ++i; // consume sync space if present

    memset(out, 0, out_cap);
    const size_t k = IR_MODEM_BITS_PER_SYMBOL;
    const size_t max_bits = out_cap * 8u;
    size_t bitcount = 0;

    // Walk (data mark, data space) pairs until the trailing lone mark / end.
    while(i < n_events && bitcount < max_bits) {
        if(!events[i].level) {
            // Unexpected space without a preceding mark; skip and resync.
            ++i;
            continue;
        }
        // events[i] is a Mark. If a Space follows, it is a data symbol; the value
        // lives in that Space. A Mark with no following Space is the end mark.
        if(i + 1 >= n_events || events[i + 1].level) {
            break; // end mark (or truncated) -> done
        }
        uint32_t level = ir_modem_space_to_level(events[i + 1].duration);
        for(size_t b = 0; b < k && bitcount < max_bits; ++b) {
            if((level >> b) & 1u) {
                out[bitcount >> 3] |= (uint8_t)(1u << (bitcount & 7u));
            }
            ++bitcount;
        }
        i += 2;
    }

    // Whole received bytes; the (< k) padding bits of the last symbol are dropped.
    const size_t len = bitcount / 8u;
    if(len == 0) return false;

    ir_modem_whiten(out, len); // de-whiten in place (XOR is self-inverse)
    *out_len = len;
    return true;
}
