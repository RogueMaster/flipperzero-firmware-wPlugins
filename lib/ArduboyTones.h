// lib/ArduboyTones.h
#pragma once
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>

#include "include/ArduboyAudioState.h"
#include "ArduboyTonesPitches.h"

using uint24_t = uint32_t;

#ifndef TONES_END
#define TONES_END 0x8000
#endif

#ifndef TONES_REPEAT
#define TONES_REPEAT 0x8001
#endif

#ifndef TONE_HIGH_VOLUME
#define TONE_HIGH_VOLUME 0x8000
#endif

#ifndef ARDUBOY_TONES_TICK_HZ
#define ARDUBOY_TONES_TICK_HZ 1000u
#endif

#define VOLUME_IN_TONE       0
#define VOLUME_ALWAYS_NORMAL 1
#define VOLUME_ALWAYS_HIGH   2

#ifndef MAX_TONES
#define MAX_TONES 3
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*((const uint16_t*)(addr)))
#endif

static constexpr float kArduboyToneSoundVolumeNormal = 1.0f;
static constexpr float kArduboyToneSoundVolumeHigh = 1.0f;
static constexpr uint32_t kArduboyToneToneTickHz = ARDUBOY_TONES_TICK_HZ;

#ifdef ARDULIB_USE_TONES
static inline uint32_t ardulib_tone_ticks_to_ms(uint16_t ticks) {
    return (uint32_t)((ticks * 1000u + (kArduboyToneToneTickHz / 2)) / kArduboyToneToneTickHz);
}

static inline float ardulib_tone_volume_for(uint16_t freq_word) {
    bool want_high = false;
    if(g_arduboy_force_norm)
        want_high = false;
    else if(g_arduboy_force_high)
        want_high = true;
    else
        want_high = ((freq_word & TONE_HIGH_VOLUME) != 0);
    return want_high ? kArduboyToneSoundVolumeHigh : kArduboyToneSoundVolumeNormal;
}

static inline uint16_t ardulib_tone_strip_volume(uint16_t freq_word) {
    return (uint16_t)(freq_word & (uint16_t)~TONE_HIGH_VOLUME);
}

void ardulib_tone_init();
void ardulib_tone_stop();
void ardulib_tone_deinit();

class ArduboyTones {
public:
    explicit ArduboyTones(bool /*enabled*/) {}
    ArduboyTones() {}
    void begin();
    static void volumeMode(uint8_t mode);
    static bool playing();
    void tones(const uint16_t* pattern);
    void tonesInRAM(uint16_t* pattern);
    void noTone();
    void tone(uint16_t frequency, uint16_t duration_ms);
    void tone(uint16_t freq, uint16_t dur_ms, uint16_t freq2, uint16_t dur2_ms);
    void tone(uint16_t f1, uint16_t d1_ms, uint16_t f2, uint16_t d2_ms, uint16_t f3, uint16_t d3_ms);
    static void nextTone() {}

private:
    static inline uint16_t ms_to_ticks16_(uint16_t ms) {
        if(ms == 0) return 0;
        uint32_t ticks = (ms * kArduboyToneToneTickHz + 500u) / 1000u;
        if(ticks == 0) ticks = 1;
        if(ticks > 0xFFFFu) ticks = 0xFFFFu;
        return (uint16_t)ticks;
    }

private:
    uint16_t inline_patterns_[8][3];
    uint8_t inline_idx_ = 0;

    uint16_t inline_patterns2_[8][5];
    uint8_t inline_idx2_ = 0;

    uint16_t inline_patterns3_[8][7];
    uint8_t inline_idx3_ = 0;
};
#else
class ArduboyTones {
public:
    explicit ArduboyTones(bool /*enabled*/) {}
    ArduboyTones() {}
    void begin() {}
    static void volumeMode(uint8_t /*mode*/) {}
    static bool playing() { return false; }
    void tones(const uint16_t* /*pattern*/) {}
    void tonesInRAM(uint16_t* /*pattern*/) {}
    void noTone() {}
    void tone(uint16_t /*frequency*/, uint16_t /*duration_ms*/) {}
    void tone(uint16_t /*freq*/, uint16_t /*dur_ms*/, uint16_t /*freq2*/, uint16_t /*dur2_ms*/) {
    }
    void tone(
        uint16_t /*f1*/,
        uint16_t /*d1_ms*/,
        uint16_t /*f2*/,
        uint16_t /*d2_ms*/,
        uint16_t /*f3*/,
        uint16_t /*d3_ms*/) {}
    static void nextTone() {}
};
#endif
