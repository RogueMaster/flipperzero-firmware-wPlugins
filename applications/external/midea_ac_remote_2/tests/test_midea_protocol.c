#include "midea_ir_protocol.h"
#include <stdio.h>

static int fails;

static uint8_t reverse8(uint8_t v) {
    v = (uint8_t)(((v & 0xF0) >> 4) | ((v & 0x0F) << 4));
    v = (uint8_t)(((v & 0xCC) >> 2) | ((v & 0x33) << 2));
    v = (uint8_t)(((v & 0xAA) >> 1) | ((v & 0x55) << 1));
    return v;
}

// IRremoteESP8266's IRMideaAC::calcChecksum, transcribed independently.
static uint8_t ref_checksum(unsigned long long state) {
    uint8_t sum = 0;
    unsigned long long t = state;
    for(uint8_t i = 0; i < 5; i++) { t >>= 8; sum = (uint8_t)(sum + reverse8((uint8_t)(t & 0xFF))); }
    sum = (uint8_t)(256 - sum);
    return reverse8(sum);
}

// Decode both phases and confirm the second is the bitwise inverse of the first.
static int decode(const uint32_t* t, size_t n, unsigned long long* out) {
    size_t i = 0;
    unsigned long long phase[2] = {0, 0};
    for(int ph = 0; ph < 2; ph++) {
        if(t[i++] != 4480 || t[i++] != 4480) { printf("bad header ph%d\n", ph); return 0; }
        unsigned long long v = 0;
        for(int b = 0; b < 48; b++) {
            if(t[i++] != 560) { printf("bad bit mark\n"); return 0; }
            uint32_t sp = t[i++];
            if(sp == 1680) v = (v << 1) | 1;
            else if(sp == 560) v = (v << 1);
            else { printf("bad space %u\n", sp); return 0; }
        }
        phase[ph] = v;
        if(ph == 0) {
            if(t[i++] != 560 || t[i++] != 5600) { printf("bad inter-phase gap\n"); return 0; }
        }
    }
    if(t[i++] != 560) { printf("bad trailing mark\n"); return 0; }
    if(i != n) { printf("length %zu != %zu\n", i, n); return 0; }
    if(phase[1] != ((~phase[0]) & 0xFFFFFFFFFFFFULL)) {
        printf("FAIL second phase is not the inverse\n"); return 0;
    }
    *out = phase[0];
    return 1;
}

int main(void) {
    uint32_t t[MIDEA_IR_MAX_TIMINGS];
    size_t n = 0;
    unsigned long long v = 0;

    // The library's reset state must satisfy the checksum model.
    unsigned long long reset = 0xA1826FFFFF62ULL;
    uint8_t got = ref_checksum(reset & ~0xFFULL), want = reset & 0xFF;
    printf("reset-state checksum: got %02X want %02X  %s\n", got, want, got == want ? "ok" : "FAIL");
    if(got != want) fails++;

    // Feature buttons must reproduce the library's published words exactly.
    struct { const char* nm; MideaToggle tg; unsigned long long want; } tog[] = {
        {"swingV", MideaToggleSwing, 0xA201FFFFFF7CULL},
        {"econo",  MideaToggleEcono, 0xA202FFFFFF7EULL},
        {"light",  MideaToggleLight, 0xA208FFFFFF75ULL},
        {"turbo",  MideaToggleTurbo, 0xA209FFFFFF74ULL},
    };
    MideaRequest req = {MideaModeCool, MideaFanAuto, 24, 0, 0};
    for(size_t k = 0; k < 4; k++) {
        if(!midea_ir_encode_toggle(&req, tog[k].tg, t, &n) || !decode(t, n, &v)) { fails++; continue; }
        int ok = v == tog[k].want;
        if(!ok) fails++;
        printf("%-8s got %012llX want %012llX  %s\n", tog[k].nm, v, tog[k].want, ok ? "ok" : "FAIL");
    }
    struct { const char* nm; MideaExtra ex; unsigned long long want; } ext[] = {
        {"quiet on",  MideaExtraQuietOn,   0xA212FFFFFF6EULL},
        {"quiet off", MideaExtraQuietOff,  0xA213FFFFFF6FULL},
        {"clean",     MideaExtraSelfClean, 0xA20DFFFFFF70ULL},
        {"8C heat",   MideaExtra8CHeat,    0xA20FFFFFFF73ULL},
    };
    for(size_t k = 0; k < 4; k++) {
        if(!midea_ir_encode_extra(&req, ext[k].ex, t, &n) || !decode(t, n, &v)) { fails++; continue; }
        int ok = v == ext[k].want;
        if(!ok) fails++;
        printf("%-9s got %012llX want %012llX  %s\n", ext[k].nm, v, ext[k].want, ok ? "ok" : "FAIL");
    }

    // Every reachable state frame
    int checked = 0;
    for(int m = MideaModeCool; m < MideaModeCount; m++) {
        for(int f = 0; f < MideaFanCount; f++) {
            for(int temp = MIDEA_TEMP_MIN; temp <= MIDEA_TEMP_MAX; temp++) {
                MideaRequest r = {(MideaMode)m, (MideaFan)f, (uint8_t)temp, 0, 0};
                if(!midea_ir_encode_state(&r, t, &n)) { printf("encode fail\n"); fails++; continue; }
                if(n != 199) { printf("FAIL len %zu != 199\n", n); fails++; continue; }
                if(!decode(t, n, &v)) { fails++; continue; }

                if(((v >> 43) & 0x1F) != 0b10100) { printf("FAIL header nibble\n"); fails++; }
                if(((v >> 40) & 0x07) != 0b001) { printf("FAIL type field\n"); fails++; }
                if(((v >> 39) & 1) != 1) { printf("FAIL power bit\n"); fails++; }
                if(((v >> 24) & 0x1F) != (unsigned)(temp - 17)) { printf("FAIL temp %d\n", temp); fails++; }
                if(((v >> 29) & 1) != 0) { printf("FAIL fahrenheit set\n"); fails++; }
                if(((v >> 16) & 0xFF) != 0xFF) { printf("FAIL byte2 not idle\n"); fails++; }
                if(((v >> 8) & 0xFF) != 0xFF) { printf("FAIL byte1 not idle\n"); fails++; }
                if((v & 0xFF) != ref_checksum(v & ~0xFFULL)) { printf("FAIL checksum\n"); fails++; }
                checked++;
            }
        }
    }
    printf("state frames verified: %d\n", checked);

    // Power off clears bit 39 and keeps the checksum valid
    if(midea_ir_encode_toggle(&req, MideaTogglePowerOff, t, &n) && decode(t, n, &v)) {
        if(((v >> 39) & 1) != 0) { printf("FAIL power-off bit\n"); fails++; }
        if((v & 0xFF) != ref_checksum(v & ~0xFFULL)) { printf("FAIL power-off checksum\n"); fails++; }
        printf("power-off frame ok (%012llX)\n", v);
    } else fails++;

    printf("\n%s\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return fails != 0;
}
