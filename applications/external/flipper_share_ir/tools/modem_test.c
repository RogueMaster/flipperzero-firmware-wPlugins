// Host round-trip test for the pure IR modem codec.
// Compiles ir_modem.c directly (no furi dependency).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ir_modem.h"

// Simple deterministic PRNG so runs are reproducible.
static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// Build RX events from encoded timings, optionally applying TSOP-like distortion:
// marks reported longer by +bias, spaces shorter by -bias, plus uniform +-jitter.
static size_t build_events(const uint32_t* t, size_t tc, IrModemEvent* ev, int bias, int jitter) {
    // start_from_mark=false layout: index0 = space, then alternating mark/space.
    size_t n = 0;
    for(size_t i = 0; i < tc; ++i) {
        bool level = (i % 2 == 1); // odd index = mark
        int d = (int)t[i];
        if(level)
            d += bias;
        else
            d -= bias;
        if(jitter) d += (int)(rng() % (2 * jitter + 1)) - jitter;
        if(d < 1) d = 1;
        ev[n].level = level;
        ev[n].duration = (uint32_t)d;
        n++;
    }
    return n;
}

static int test_one(size_t len, int bias, int jitter, const char* label) {
    uint8_t in[256], out[256];
    for(size_t i = 0; i < len; ++i)
        in[i] = (uint8_t)(rng() & 0xFF);
    // also test a few pathological all-same payloads
    static int alt = 0;
    if((alt++ & 3) == 0) memset(in, 0x00, len);
    if((alt & 3) == 1) memset(in, 0xFF, len);

    uint32_t timings[2048];
    size_t tc = ir_modem_encode(in, len, timings, 2048);
    if(tc == 0) {
        printf("  [%s len=%zu] ENCODE FAILED\n", label, len);
        return 1;
    }

    IrModemEvent ev[2048];
    size_t nev = build_events(timings, tc, ev, bias, jitter);

    size_t out_len = 0;
    bool ok = ir_modem_decode(ev, nev, out, sizeof(out), &out_len);
    if(!ok) {
        printf("  [%s len=%zu] DECODE FAILED\n", label, len);
        return 1;
    }
    if(out_len != len) {
        printf("  [%s len=%zu] LEN MISMATCH got %zu\n", label, len, out_len);
        return 1;
    }
    if(memcmp(in, out, len) != 0) {
        printf("  [%s len=%zu] DATA MISMATCH\n", label, len);
        for(size_t i = 0; i < len; ++i)
            if(in[i] != out[i]) {
                printf("    byte %zu: %02X != %02X\n", i, in[i], out[i]);
            }
        return 1;
    }
    return 0;
}

int main(void) {
    size_t lens[] = {1, 5, 13, 51, 52, 56, 60, 61, 64, 87};
    int fails = 0, total = 0;

    printf(
        "bits/sym=%u levels=%u mark=%u base=%u step=%u maxspace=%u rxto=%u\n",
        IR_MODEM_BITS_PER_SYMBOL,
        IR_MODEM_LEVELS,
        IR_MODEM_MARK_US,
        IR_MODEM_SPACE_BASE_US,
        IR_MODEM_SPACE_STEP_US,
        IR_MODEM_SPACE_BASE_US + (IR_MODEM_LEVELS - 1) * IR_MODEM_SPACE_STEP_US,
        IR_MODEM_RX_TIMEOUT_US);

    // Clean channel.
    printf("Clean channel:\n");
    for(size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); ++i) {
        for(int r = 0; r < 20; ++r) {
            total++;
            fails += test_one(lens[i], 0, 0, "clean");
        }
    }
    // Realistic TSOP distortion: bias +-60us on marks/spaces, +-80us jitter.
    printf("With bias=60 jitter=80:\n");
    for(size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); ++i) {
        for(int r = 0; r < 50; ++r) {
            total++;
            fails += test_one(lens[i], 60, 80, "distort");
        }
    }
    // Harsher: bias 100, jitter 120 (~step/3). Should still mostly pass.
    printf("With bias=100 jitter=120:\n");
    int harsh_fail = 0, harsh_total = 0;
    for(size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); ++i) {
        for(int r = 0; r < 50; ++r) {
            harsh_total++;
            harsh_fail += test_one(lens[i], 100, 120, "harsh");
        }
    }

    printf(
        "\nRESULT: clean+distort %d/%d failed; harsh %d/%d failed\n",
        fails,
        total,
        harsh_fail,
        harsh_total);
    return fails ? 1 : 0;
}
