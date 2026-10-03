#include "ac_decode.h"

#include <string.h>

// Infrared receivers hold their output low a little longer than the carrier
// actually lasts, so marks come back long and spaces short by roughly the same
// amount - a hundred microseconds or so, depending on the receiver and how
// bright the remote is.
//
// The parser absorbs that by using windows wide enough to cover it in either
// direction. The scorer does something better: it compares mark-plus-space
// periods, which the bias cancels out of exactly, so a stretched capture
// ranks candidates the same way a clean one does. An earlier version instead
// subtracted a fixed excess before comparing, which quietly handed clean
// signals to the wrong protocol - it moved a real 325 us zero-space onto a
// neighbouring entry's nominal 385.

#define AC_TOL_PCT     32
#define AC_TOL_MIN_US  170
#define AC_HDR_TOL_PCT 25
#define AC_HDR_TOL_US  400

/// Guards for the fallback parse that decides whether an unrecognised capture
/// was a remote at all.
#define AC_GEN_MIN_BITS       20
#define AC_GEN_MARK_MIN_US    200
#define AC_GEN_MARK_MAX_US    1400
#define AC_GEN_HDR_MIN_US     1500
#define AC_GEN_HDR_MAX_US     30000
#define AC_GEN_MARK_TOL_PCT   40
#define AC_GEN_SPACE_TOL_PCT  40
/// Share of marks that must sit near the median, in percent. Lamp flicker and
/// switching-supply hash fail this long before anything else.
#define AC_GEN_MARK_AGREE_PCT 88
/// Marks sampled to find the median bit-mark width. More than enough: real
/// frames hold hundreds of identical marks, and noise fails on the first few.
#define AC_GEN_SAMPLE         128

// ---------------------------------------------------------------- matching

static bool near_us(uint32_t got, uint32_t want, uint32_t tol_pct, uint32_t tol_min) {
    if(want == 0) return false;
    uint32_t tol = want * tol_pct / 100;
    if(tol < tol_min) tol = tol_min;
    return (got + tol >= want) && (got <= want + tol);
}

static bool near_mark(uint32_t got, uint32_t want) {
    return near_us(got, want, AC_TOL_PCT, AC_TOL_MIN_US);
}

static bool near_space(uint32_t got, uint32_t want) {
    return near_us(got, want, AC_TOL_PCT, AC_TOL_MIN_US);
}

static bool near_hdr_mark(uint32_t got, uint32_t want) {
    return near_us(got, want, AC_HDR_TOL_PCT, AC_HDR_TOL_US);
}

static bool near_hdr_space(uint32_t got, uint32_t want) {
    return near_us(got, want, AC_HDR_TOL_PCT, AC_HDR_TOL_US);
}

/// Distance from the expected value, in parts per thousand.
static uint32_t rel_err(uint32_t got, uint32_t want) {
    if(want == 0) return 0;
    uint32_t d = got > want ? got - want : want - got;
    return d * 1000u / want;
}

// ------------------------------------------------------------------ parser

typedef struct {
    uint8_t data[AC_MAX_BYTES];
    uint16_t bits; ///< true bits on the wire
    uint16_t pack_bits; ///< write cursor, realigned at every section
    uint8_t sections;
    bool overflow;

    uint32_t hm_sum, hs_sum;
    uint16_t hdr_count;
    uint32_t bm_sum;
    uint16_t bm_count;
    uint32_t one_sum, zero_sum;
    uint16_t one_count, zero_count;
} AcParse;

static void parse_align(AcParse* p) {
    p->pack_bits = (uint16_t)((p->pack_bits + 7u) & ~7u);
}

static void parse_push(AcParse* p, bool bit, bool msb_first) {
    p->bits++;
    uint16_t idx = (uint16_t)(p->pack_bits >> 3);
    if(idx >= AC_MAX_BYTES) {
        p->overflow = true;
        return;
    }
    if(bit) {
        uint8_t off = (uint8_t)(p->pack_bits & 7u);
        p->data[idx] |= msb_first ? (uint8_t)(0x80u >> off) : (uint8_t)(1u << off);
    }
    p->pack_bits++;
}

/// Walk the capture as if it were `e`. Returns false the moment something
/// does not fit, which is what keeps a wrong entry from claiming a signal.
static bool ac_parse(const uint32_t* t, size_t n, const AcProtoEntry* e, AcParse* p) {
    memset(p, 0, sizeof(*p));

    size_t i = 0;

    // A few remotes open with one or more bursts that are not the header
    // proper. Daikin's BRC52B63 and DGS01 send two of them, so consume as
    // many as are actually there rather than assuming one.
    if(e->lead_mark) {
        while(i + 1 < n && near_hdr_mark(t[i], e->lead_mark) &&
              near_hdr_space(t[i + 1], e->lead_space)) {
            i += 2;
        }
    }

    while(i + 1 < n) {
        uint32_t mark = t[i];
        uint32_t space = t[i + 1];

        if(near_hdr_mark(mark, e->hdr_mark) && near_hdr_space(space, e->hdr_space)) {
            p->hm_sum += mark;
            p->hs_sum += space;
            p->hdr_count++;
            p->sections++;
            parse_align(p);
            i += 2;
            continue;
        }

        if(!near_mark(mark, e->bit_mark)) return false;

        if(near_space(space, e->one_space)) {
            parse_push(p, true, e->msb_first);
            p->one_sum += space;
            p->one_count++;
        } else if(near_space(space, e->zero_space)) {
            parse_push(p, false, e->msb_first);
            p->zero_sum += space;
            p->zero_count++;
        } else if(space > (uint32_t)e->one_space + e->zero_space + AC_TOL_MIN_US) {
            // The mark closed a section; the long space is the gap.
            //
            // The threshold is relative to the protocol rather than a fixed
            // number of microseconds. Samsung's inter-section gap is 2886 us,
            // shorter than some protocols' one-space, so a single global
            // constant either misses it or starts swallowing real data bits.
            // A gap always clears one-space plus zero-space; a data bit never
            // does, and both are checked first anyway.
            parse_align(p);
            i += 2;
            continue;
        } else {
            return false;
        }

        p->bm_sum += mark;
        p->bm_count++;
        i += 2;
    }

    // The trailing stop mark is expected. Most protocols close with a bit
    // mark, but Daikin's BRC52B63 and DGS01 close with a header-length one,
    // so accept either rather than rejecting the whole frame over its last
    // pulse.
    if(i < n && !near_mark(t[i], e->bit_mark) && !near_hdr_mark(t[i], e->hdr_mark) &&
       !(e->lead_mark && near_hdr_mark(t[i], e->lead_mark))) {
        return false;
    }

    return p->hdr_count > 0 && p->bits > 0 && !p->overflow;
}

// --------------------------------------------------------------- signature

static bool sig_at(const AcParse* p, const AcProtoEntry* e, uint16_t off) {
    if(off + e->sig_len > (p->pack_bits >> 3)) return false;
    for(uint8_t k = 0; k < e->sig_len; k++) {
        if((p->data[off + k] & e->sig_mask[k]) != e->sig[k]) return false;
    }
    return true;
}

/// Coolix sends every payload byte twice, the second time inverted. Nothing
/// else in the database does, which is the only thing that separates it from
/// Midea - the two share a line code and a bit count.
static bool sig_coolix(const AcParse* p) {
    if(p->pack_bits < 48) return false;
    for(uint8_t k = 0; k < 6; k += 2) {
        if(p->data[k] != (uint8_t)~p->data[k + 1]) return false;
    }
    return true;
}

/// Midea sends its six payload bytes and then the same six inverted, under a
/// fixed 0b10100 header. Toshiba's twelve-byte frame is the same length with
/// almost the same line code, so this is what keeps them apart.
static bool sig_midea(const AcParse* p) {
    if(p->pack_bits < 96) return false;
    if((p->data[0] & 0xF8) != 0xA0) return false;
    for(uint8_t k = 0; k < 6; k++) {
        if(p->data[k] != (uint8_t)~p->data[k + 6]) return false;
    }
    return true;
}

/// Goodweather puts every byte on the wire twice, the second time inverted,
/// so a 96-bit frame carries only 48 bits of payload.
static bool sig_byte_pairs(const AcParse* p) {
    uint16_t bytes = (uint16_t)(p->pack_bits >> 3);
    if(bytes < 2 || (bytes & 1)) return false;
    for(uint16_t k = 0; k + 1 < bytes; k += 2) {
        if(p->data[k] != (uint8_t)~p->data[k + 1]) return false;
    }
    return true;
}

/// Neoclima ends with a fixed 0xA5 and a sum of everything before it. Without
/// this the entry claims any frame that merely has a similar header and the
/// same bit count - which is how a Goodweather capture came back as Neoclima.
static bool sig_neoclima(const AcParse* p) {
    if((p->pack_bits >> 3) < 12) return false;
    if(p->data[10] != 0xA5) return false;
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 11; i++) {
        sum = (uint8_t)(sum + p->data[i]);
    }
    return p->data[11] == sum;
}

static bool sig_check(const AcParse* p, const AcProtoEntry* e) {
    switch(e->sig_kind) {
    case AcSigCoolix:
        return sig_coolix(p);
    case AcSigMidea:
        return sig_midea(p);
    case AcSigBytePairs:
        return sig_byte_pairs(p);
    case AcSigNeoclima:
        return sig_neoclima(p);
    case AcSigPrefix:
        // A protocol with a preamble - Daikin opens with five stray bits -
        // pushes its signature past the first byte.
        for(uint16_t off = 0; off <= 2; off++) {
            if(sig_at(p, e, off)) return true;
        }
        return false;
    default:
        return false;
    }
}

// ------------------------------------------------------------- repeat check

/// A capture often holds the same frame several times over; the receiver only
/// stops after 150 ms of silence. Returns the repeat count, or 0 when the
/// copies are not identical and so this is not a repeat at all.
static uint8_t repeat_count(const AcParse* p, uint16_t frame_bits) {
    if(frame_bits == 0 || p->bits % frame_bits) return 0;
    uint16_t k = p->bits / frame_bits;
    if(k == 0 || k > 8) return 0;
    if(k == 1) return 1;

    uint16_t total_bytes = (uint16_t)(p->pack_bits >> 3);
    if(total_bytes % k) return 0;
    uint16_t frame_bytes = total_bytes / k;
    if(frame_bytes == 0) return 0;

    for(uint16_t r = 1; r < k; r++) {
        if(memcmp(p->data, p->data + (size_t)r * frame_bytes, frame_bytes)) return 0;
    }
    return (uint8_t)k;
}

// ------------------------------------------------------------ generic parse

static uint32_t median_of(uint32_t* v, uint16_t n) {
    // Insertion sort. n is small and this runs once per capture.
    for(uint16_t i = 1; i < n; i++) {
        uint32_t key = v[i];
        int32_t j = (int32_t)i - 1;
        while(j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
    return v[n / 2];
}

/// Decide whether an unmatched capture still looks like a remote control.
/// This is the noise gate: it wants a real header, bit marks that agree with
/// each other, and spaces that fall into two clean groups.
static bool ac_looks_like_remote(const uint32_t* t, size_t n, AcDetection* out) {
    if(n < AC_MIN_TIMINGS) return false;

    uint32_t hm = t[0];
    uint32_t hs = t[1];
    if(hm < AC_GEN_HDR_MIN_US || hm > AC_GEN_HDR_MAX_US) return false;
    if(hs < 400 || hs > AC_GEN_HDR_MAX_US) return false;

    // Sample the marks that follow the header. A slice is enough to find the
    // median, and it keeps this off the heap - a full-capture copy would cost
    // four kilobytes of the app's allocation.
    uint32_t sample[AC_GEN_SAMPLE];
    uint16_t ns_taken = 0;
    uint16_t nm = 0; // bits, i.e. mark/space pairs that carry data

    for(size_t i = 2; i + 1 < n; i += 2) {
        uint32_t mark = t[i];
        uint32_t space = t[i + 1];
        if(space > AC_SECTION_GAP_US) continue; // gap or a repeated header
        if(mark > AC_GEN_HDR_MIN_US) continue; // a header of a later section
        if(ns_taken < AC_GEN_SAMPLE) sample[ns_taken++] = mark;
        nm++;
    }
    if(nm < AC_GEN_MIN_BITS) return false;

    uint32_t bm = median_of(sample, ns_taken);
    if(bm < AC_GEN_MARK_MIN_US || bm > AC_GEN_MARK_MAX_US) return false;

    uint16_t agree = 0;
    for(uint16_t i = 0; i < ns_taken; i++) {
        if(near_us(sample[i], bm, AC_GEN_MARK_TOL_PCT, AC_TOL_MIN_US)) agree++;
    }
    if((uint32_t)agree * 100u / ns_taken < AC_GEN_MARK_AGREE_PCT) return false;

    // Split the spaces at the midpoint between the shortest and the longest,
    // then demand each half be tight around its own mean. Real pulse-distance
    // coding falls into exactly two groups; noise does not. Walking the
    // capture three times costs less than keeping a copy of it.
    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    for(size_t i = 2; i + 1 < n; i += 2) {
        if(t[i + 1] > AC_SECTION_GAP_US || t[i] > AC_GEN_HDR_MIN_US) continue;
        if(t[i + 1] < lo) lo = t[i + 1];
        if(t[i + 1] > hi) hi = t[i + 1];
    }
    if(lo > hi) return false;

    uint32_t mid = (lo + hi) / 2;
    uint32_t one_sum = 0, zero_sum = 0;
    uint16_t one_n = 0, zero_n = 0;
    for(size_t i = 2; i + 1 < n; i += 2) {
        if(t[i + 1] > AC_SECTION_GAP_US || t[i] > AC_GEN_HDR_MIN_US) continue;
        if(t[i + 1] > mid) {
            one_sum += t[i + 1];
            one_n++;
        } else {
            zero_sum += t[i + 1];
            zero_n++;
        }
    }
    if(zero_n == 0) return false;
    uint32_t zero = zero_sum / zero_n;
    uint32_t one = one_n ? one_sum / one_n : zero;

    for(size_t i = 2; i + 1 < n; i += 2) {
        if(t[i + 1] > AC_SECTION_GAP_US || t[i] > AC_GEN_HDR_MIN_US) continue;
        uint32_t want = t[i + 1] > mid ? one : zero;
        if(!near_us(t[i + 1], want, AC_GEN_SPACE_TOL_PCT, AC_TOL_MIN_US)) return false;
    }

    out->hdr_mark = (uint16_t)(hm > 0xFFFF ? 0xFFFF : hm);
    out->hdr_space = (uint16_t)(hs > 0xFFFF ? 0xFFFF : hs);
    out->bit_mark = (uint16_t)bm;
    out->one_space = (uint16_t)one;
    out->zero_space = (uint16_t)zero;
    out->bits = nm;
    return true;
}

// -------------------------------------------------------------------- main

typedef struct {
    const AcProtoEntry* entry;
    bool signature_ok;
    bool bits_exact;
    uint32_t err;
    uint8_t repeats;
    AcParse parse;
} AcCandidate;

/// Better means: signature confirmed beats no signature; an exact bit count
/// beats a multiple of it; then the closest timings win.
static bool candidate_better(const AcCandidate* a, const AcCandidate* b) {
    if(!b->entry) return true;
    if(a->signature_ok != b->signature_ok) return a->signature_ok;
    if(a->bits_exact != b->bits_exact) return a->bits_exact;
    return a->err < b->err;
}

bool ac_decode(const uint32_t* timings, size_t count, AcDetection* out) {
    if(!timings || !out) return false;

    memset(out, 0, sizeof(*out));
    out->kind = AcResultNoise;
    out->repeats = 1;
    out->timings_count = (uint16_t)(count > 0xFFFF ? 0xFFFF : count);

    uint32_t duration = 0;
    for(size_t i = 0; i < count; i++) {
        duration += timings[i];
    }
    out->duration_us = duration;

    if(count < AC_MIN_TIMINGS) return false;

    AcCandidate best;
    memset(&best, 0, sizeof(best));

    for(size_t idx = 0; idx < ac_proto_db_count; idx++) {
        const AcProtoEntry* e = &ac_proto_db[idx];

        AcCandidate c;
        memset(&c, 0, sizeof(c));
        if(!ac_parse(timings, count, e, &c.parse)) continue;

        uint16_t frame_bits = e->bits ? e->bits : c.parse.bits;
        c.repeats = repeat_count(&c.parse, frame_bits);
        if(c.repeats == 0) continue;

        c.entry = e;
        c.bits_exact = (e->bits != 0);
        c.signature_ok = sig_check(&c.parse, e);

        uint32_t hm = c.parse.hdr_count ? c.parse.hm_sum / c.parse.hdr_count : 0;
        uint32_t hs = c.parse.hdr_count ? c.parse.hs_sum / c.parse.hdr_count : 0;
        uint32_t bm = c.parse.bm_count ? c.parse.bm_sum / c.parse.bm_count : 0;
        uint32_t one = c.parse.one_count ? c.parse.one_sum / c.parse.one_count : e->one_space;
        uint32_t zero = c.parse.zero_count ? c.parse.zero_sum / c.parse.zero_count : e->zero_space;

        // Periods are immune to the receiver's mark/space bias: whatever it
        // adds to a mark it takes off the space that follows. The two
        // standalone terms are on values large enough that a hundred
        // microseconds of bias barely moves them, and they separate protocols
        // that share a period but split it differently.
        c.err = rel_err(hm + hs, (uint32_t)e->hdr_mark + e->hdr_space) +
                rel_err(hs, e->hdr_space) +
                2 * rel_err(bm + one, (uint32_t)e->bit_mark + e->one_space) +
                2 * rel_err(bm + zero, (uint32_t)e->bit_mark + e->zero_space) +
                rel_err(one, e->one_space);

        if(candidate_better(&c, &best)) best = c;
    }

    if(best.entry) {
        const AcParse* p = &best.parse;
        out->kind = AcResultMatch;
        out->entry = best.entry;
        out->bits = p->bits;
        out->repeats = best.repeats;
        out->sections = p->sections;
        out->signature_ok = best.signature_ok;

        out->hdr_mark = (uint16_t)(p->hdr_count ? p->hm_sum / p->hdr_count : 0);
        out->hdr_space = (uint16_t)(p->hdr_count ? p->hs_sum / p->hdr_count : 0);
        out->bit_mark = (uint16_t)(p->bm_count ? p->bm_sum / p->bm_count : 0);
        out->one_space = (uint16_t)(p->one_count ? p->one_sum / p->one_count : 0);
        out->zero_space = (uint16_t)(p->zero_count ? p->zero_sum / p->zero_count : 0);

        uint16_t total_bytes = (uint16_t)(p->pack_bits >> 3);
        uint16_t len = best.repeats ? total_bytes / best.repeats : total_bytes;
        if(len == 0) len = total_bytes;
        if(len > AC_MAX_BYTES) len = AC_MAX_BYTES;
        memcpy(out->data, p->data, len);
        out->data_len = len;
        return true;
    }

    if(ac_looks_like_remote(timings, count, out)) {
        out->kind = AcResultUnknown;
        return true;
    }

    out->kind = AcResultNoise;
    return false;
}

void ac_detection_format_hex(const AcDetection* d, char* out, size_t len) {
    static const char hex[] = "0123456789ABCDEF";
    if(!out || len == 0) return;
    out[0] = '\0';
    if(!d) return;

    size_t pos = 0;
    for(uint16_t i = 0; i < d->data_len; i++) {
        if(pos + 4 >= len) break;
        if(i) out[pos++] = ' ';
        out[pos++] = hex[d->data[i] >> 4];
        out[pos++] = hex[d->data[i] & 0x0F];
    }
    out[pos] = '\0';
}
