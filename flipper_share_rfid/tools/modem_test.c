// Host test harness for the RFID Manchester-ASK modem (no hardware, no furi).
// Build: cc -Wall -Wextra -O2 tools/modem_test.c rfid_modem.c -o /tmp/modem_test
//
// Encodes frames, converts the encoder's (duration, pulse) carrier-cycle pairs
// into (level, duration_us) capture runs the way the RFID reader would see them,
// optionally perturbs them, and checks the decoder recovers the original packet.

#include "../rfid_modem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PACKET_MAX 73 // FSH_PACKET_MAX for this app (FSH_DATA_LENGTH = 64)

typedef struct {
    int level;
    long dur_us; // signed so jitter can push it around without underflow surprises
} Event;

// Convert an encoded frame into capture events. Returns event count.
static int encode_to_events(const uint8_t* packet, size_t len, Event* ev, int cap) {
    RfidModemEnc enc;
    rfid_modem_enc_set_frame(&enc, packet, len);
    int n = 0;
    uint32_t dur, pulse;
    while(rfid_modem_enc_next(&enc, &dur, &pulse)) {
        uint32_t high = pulse;
        uint32_t low = dur - pulse;
        if(high > 0) {
            if(n < cap) {
                ev[n].level = 1;
                ev[n].dur_us = (long)high * RFID_MODEM_CARRIER_US;
                n++;
            }
        }
        if(low > 0) {
            if(n < cap) {
                ev[n].level = 0;
                ev[n].dur_us = (long)low * RFID_MODEM_CARRIER_US;
                n++;
            }
        }
    }
    return n;
}

// Coalesce adjacent same-level events (hardware capture merges same-level runs).
static int coalesce(Event* ev, int n) {
    if(n == 0) return 0;
    int w = 0;
    for(int i = 1; i < n; i++) {
        if(ev[i].level == ev[w].level) {
            ev[w].dur_us += ev[i].dur_us;
        } else {
            ev[++w] = ev[i];
        }
    }
    return w + 1;
}

// Feed events through the decoder; capture the first packet it returns.
// Returns decoded length (0 if none), writes into out.
static size_t decode_events(const Event* ev, int n, uint8_t* out, size_t cap) {
    RfidModemDec dec;
    rfid_modem_dec_reset(&dec);
    for(int i = 0; i < n; i++) {
        if(ev[i].dur_us <= 0) continue;
        size_t r = rfid_modem_dec_feed(&dec, ev[i].level, (uint32_t)ev[i].dur_us, out, cap);
        if(r) return r;
    }
    return 0;
}

static int roundtrip(const uint8_t* packet, size_t len, long jitter, int invert) {
    static Event ev[8192];
    int n = encode_to_events(packet, len, ev, 8192);
    // Append a trailing gap so the last frame closes cleanly on its own bits.
    if(n < 8192) {
        ev[n].level = 0;
        ev[n].dur_us = 2000;
        n++;
    }
    if(invert)
        for(int i = 0; i < n; i++) ev[i].level = !ev[i].level;
    if(jitter)
        for(int i = 0; i < n; i++) {
            long j = (rand() % (2 * jitter + 1)) - jitter;
            ev[i].dur_us += j;
        }
    n = coalesce(ev, n);

    uint8_t out[TEST_PACKET_MAX + 8];
    size_t r = decode_events(ev, n, out, sizeof(out));
    if(r != len) return 0;
    return memcmp(out, packet, len) == 0;
}

static void fill_random(uint8_t* p, size_t len) {
    for(size_t i = 0; i < len; i++) p[i] = (uint8_t)(rand() & 0xFF);
}

int main(void) {
    srand(0xC0FFEE);
    int fails = 0, total = 0;
    uint8_t buf[TEST_PACKET_MAX];

    // ---- Fixed vectors, clean channel ----
    struct {
        const char* name;
        size_t len;
        int fill;
    } fixed[] = {
        {"1-byte", 1, -1},
        {"61-byte CTRL", 61, -1},
        {"73-byte DATA", 73, -1},
        {"all-0x00", 73, 0x00},
        {"all-0xFF", 73, 0xFF},
    };
    for(size_t f = 0; f < sizeof(fixed) / sizeof(fixed[0]); f++) {
        if(fixed[f].fill < 0)
            fill_random(buf, fixed[f].len);
        else
            memset(buf, fixed[f].fill, fixed[f].len);
        total++;
        int ok = roundtrip(buf, fixed[f].len, 0, 0);
        if(!ok) {
            fails++;
            printf("FAIL clean: %s\n", fixed[f].name);
        }
    }

    // ---- 10000 random frames, clean channel ----
    int clean_fail = 0;
    for(int i = 0; i < 10000; i++) {
        size_t len = 1 + (rand() % TEST_PACKET_MAX);
        fill_random(buf, len);
        total++;
        if(!roundtrip(buf, len, 0, 0)) {
            clean_fail++;
            if(clean_fail <= 3) printf("FAIL clean random len=%zu\n", len);
        }
    }
    fails += clean_fail;
    printf("clean random: %d/%d failed\n", clean_fail, 10000);

    // ---- Realistic jitter +/-30us ----
    int jit_fail = 0;
    for(int i = 0; i < 10000; i++) {
        size_t len = 1 + (rand() % TEST_PACKET_MAX);
        fill_random(buf, len);
        total++;
        if(!roundtrip(buf, len, 30, 0)) {
            jit_fail++;
            if(jit_fail <= 3) printf("FAIL jitter random len=%zu\n", len);
        }
    }
    fails += jit_fail;
    printf("jitter +/-30us: %d/%d failed\n", jit_fail, 10000);

    // ---- Polarity-inverted stream ----
    int pol_fail = 0;
    for(int i = 0; i < 2000; i++) {
        size_t len = 1 + (rand() % TEST_PACKET_MAX);
        fill_random(buf, len);
        total++;
        if(!roundtrip(buf, len, 0, 1)) {
            pol_fail++;
            if(pol_fail <= 3) printf("FAIL polarity random len=%zu\n", len);
        }
    }
    fails += pol_fail;
    printf("polarity-inverted: %d/%d failed\n", pol_fail, 2000);

    // ---- Truncated frame + next frame: decoder must recover the second ----
    {
        static Event ev[8192];
        uint8_t a[40], b[50];
        fill_random(a, sizeof(a));
        fill_random(b, sizeof(b));
        int na = encode_to_events(a, sizeof(a), ev, 8192);
        na = na / 2; // truncate frame A mid-stream
        // dropout gap after the truncation
        ev[na].level = 0;
        ev[na].dur_us = 3000;
        na++;
        int nb = encode_to_events(b, sizeof(b), ev + na, 8192 - na);
        int n = na + nb;
        if(n < 8192) {
            ev[n].level = 0;
            ev[n].dur_us = 2000;
            n++;
        }
        n = coalesce(ev, n);
        uint8_t out[128];
        size_t r = decode_events(ev, n, out, sizeof(out));
        total++;
        int ok = (r == sizeof(b)) && (memcmp(out, b, sizeof(b)) == 0);
        if(!ok) {
            fails++;
            printf("FAIL resync after truncation (got len=%zu)\n", r);
        } else {
            printf("resync after truncation: OK\n");
        }
    }

    // ---- Harsh jitter (informational: expected to degrade) ----
    int harsh_fail = 0;
    for(int i = 0; i < 5000; i++) {
        size_t len = 1 + (rand() % TEST_PACKET_MAX);
        fill_random(buf, len);
        if(!roundtrip(buf, len, 45, 0)) harsh_fail++;
    }
    printf("harsh jitter +/-45us (informational): %d/%d failed\n", harsh_fail, 5000);

    printf("\n==== %d/%d mandatory checks failed ====\n", fails, total);
    return fails ? 1 : 0;
}
