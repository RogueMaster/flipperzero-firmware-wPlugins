// game/ArduboyTonesFX.h
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "lib/Arduino.h"
#include "lib/ArduboyAudio.h"
#include "lib/ArduboyTones.h"
#include "lib/ArduboyFX.h"

#ifndef ARDUBOY_TONESFX_MAX_WORDS
#define ARDUBOY_TONESFX_MAX_WORDS 512
#endif

class ArduboyTonesFX {
public:
    ArduboyTonesFX();
    ArduboyTonesFX(uint16_t* tonesArray, uint8_t tonesArrayLen);

    static void attachTones(ArduboyTones* backend);

    static void tone(uint16_t freq, uint16_t dur = 0);
    static void tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2);
    static void tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2, uint16_t f3, uint16_t d3);

    static void tones(const uint16_t* pattern);
    static void tonesInRAM(uint16_t* pattern);
    static void tonesFromFX(uint32_t fx_addr);

    static void noTone();
    static void volumeMode(uint8_t mode);
    static bool playing();

private:
    static ArduboyTones* tonesBackend;
    static ArduboyTones tonesBackendInternal;

    static uint16_t toneSequence[MAX_TONES * 2 + 1];
    static uint16_t seqBuffer[ARDUBOY_TONESFX_MAX_WORDS];

    static inline bool isOutputEnabledNow() {
        return ArduboyAudio::enabled();
    }

    static inline void ensureBackendPointer() {
        if(!tonesBackend) tonesBackend = &tonesBackendInternal;
    }

    static inline uint16_t dur1024_to_ticks(uint16_t dur1024) {
        if(dur1024 == 0) return 0;
        uint32_t ticks = (uint32_t)dur1024 * (uint32_t)ARDUBOY_TONES_TICK_HZ + 512u;
        ticks >>= 10;
        if(ticks == 0) ticks = 1;
        if(ticks > 0xFFFFu) ticks = 0xFFFFu;
        return (uint16_t)ticks;
    }

    static uint16_t copyPattern(const uint16_t* src, bool progmem);
    static uint16_t decodeFx(uint32_t fx_addr);
};
