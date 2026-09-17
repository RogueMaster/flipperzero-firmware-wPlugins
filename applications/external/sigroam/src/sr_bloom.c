#include "sr_bloom.h"

#include <string.h>

/*
 * ★ Pure logic; must not include furi (ADR-003).
 *
 * The FNV-1a 32-bit constants and operation order follow Landon Curt Noll's official page and
 * Go's hash/fnv, checked 2026-08-16 (the sources section of task card T2.5). The 32-bit
 * multiplication overflow is the intended behavior.
 */

enum {
    SR_FNV1A_OFFSET = 2166136261u, /* 0x811C9DC5 */
    SR_FNV1A_PRIME = 16777619u     /* 0x01000193 = 2^24 + 2^8 + 0x93 */
};

_Static_assert(2166136261u == 0x811C9DC5u, "FNV-1a offset basis");
_Static_assert(16777619u == 0x01000193u, "FNV-1a prime");

static unsigned char bloom_fold(unsigned char c) {
    if(c >= 'a' && c <= 'f') {
        return (unsigned char)(c - ('a' - 'A'));
    }
    return c;
}

static uint32_t fnv1a32(const char* s, uint32_t basis) {
    uint32_t h = basis;
    const unsigned char* p = (const unsigned char*)s;

    while(*p != 0) {
        h ^= bloom_fold(*p);
        h *= SR_FNV1A_PRIME;
        p++;
    }
    return h;
}

static void bloom_indices(const char* bssid, uint32_t out[SR_BLOOM_K]) {
    uint32_t h1 = fnv1a32(bssid, SR_FNV1A_OFFSET);
    /* Same input, different offset: the second pass seeds with h1. Do not use a shift of h1 as h2. */
    uint32_t h2 = fnv1a32(bssid, h1);
    unsigned i;

    /*
     * m is a power of two, so an even h2 would make the probe sequence cover only some slots.
     * Seeding the second FNV with h1 makes the LSB always 1 (the prime is odd =>
     * LSB(h) = LSB(basis) XOR the LSB of each byte; LSB(h1) = 1 XOR P,
     * LSB(h2) = (1 XOR P) XOR P = 1), but it is still forced odd as the task card requires.
     */
    h2 |= 1u;

    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        out[i] = (h1 + (uint32_t)i * h2) & (uint32_t)(SR_BLOOM_BITS - 1);
    }
}

static void bit_set(uint8_t* bits, uint32_t idx) {
    bits[idx >> 3] |= (uint8_t)(1u << (idx & 7u));
}

static bool bit_test(const uint8_t* bits, uint32_t idx) {
    return (bits[idx >> 3] & (uint8_t)(1u << (idx & 7u))) != 0;
}

void sr_bloom_init(SrBloom* b) {
    if(b == NULL) {
        return;
    }
    memset(b, 0, sizeof(*b));
}

bool sr_bloom_add(SrBloom* b, const char* bssid) {
    uint32_t idx[SR_BLOOM_K];
    unsigned i;
    bool fresh;

    if(b == NULL || bssid == NULL || bssid[0] == '\0') {
        return false;
    }

    bloom_indices(bssid, idx);

    fresh = false;
    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        if(!bit_test(b->bits, idx[i])) {
            fresh = true;
        }
        bit_set(b->bits, idx[i]);
    }

    b->added++;
    if(fresh) {
        b->unique++;
        return true;
    }
    return false;
}

bool sr_bloom_maybe_contains(const SrBloom* b, const char* bssid) {
    uint32_t idx[SR_BLOOM_K];
    unsigned i;

    if(b == NULL || bssid == NULL || bssid[0] == '\0') {
        return false;
    }

    bloom_indices(bssid, idx);
    for(i = 0; i < (unsigned)SR_BLOOM_K; i++) {
        if(!bit_test(b->bits, idx[i])) {
            return false;
        }
    }
    return true;
}

uint32_t sr_bloom_unique(const SrBloom* b) {
    return b == NULL ? 0u : b->unique;
}

uint32_t sr_bloom_fill_bits(const SrBloom* b) {
    uint32_t n = 0;
    size_t i;

    if(b == NULL) {
        return 0;
    }
    for(i = 0; i < (size_t)SR_BLOOM_BYTES; i++) {
        unsigned v = b->bits[i];
        while(v != 0) {
            n += v & 1u;
            v >>= 1;
        }
    }
    return n;
}
