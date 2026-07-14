// game/ArduboyTonesFX.cpp
#include "game/ArduboyTonesFX.h"

ArduboyTones* ArduboyTonesFX::tonesBackend = nullptr;
ArduboyTones ArduboyTonesFX::tonesBackendInternal;

uint16_t ArduboyTonesFX::toneSequence[MAX_TONES * 2 + 1] = {0};
uint16_t ArduboyTonesFX::seqBuffer[ARDUBOY_TONESFX_MAX_WORDS] = {0};

ArduboyTonesFX::ArduboyTonesFX() {
    toneSequence[MAX_TONES * 2] = TONES_END;
}

ArduboyTonesFX::ArduboyTonesFX(uint16_t* /*tonesArray*/, uint8_t /*tonesArrayLen*/) {
    toneSequence[MAX_TONES * 2] = TONES_END;
}

void ArduboyTonesFX::attachTones(ArduboyTones* backend) {
    tonesBackend = backend;
}

void ArduboyTonesFX::tone(uint16_t freq, uint16_t dur) {
    if(!isOutputEnabledNow()) return;
    ensureBackendPointer();

    toneSequence[0] = freq;
    toneSequence[1] = dur1024_to_ticks(dur);
    toneSequence[2] = TONES_END;

    tonesBackend->tonesInRAM(toneSequence);
}

void ArduboyTonesFX::tone(uint16_t f1, uint16_t d1, uint16_t f2, uint16_t d2) {
    if(!isOutputEnabledNow()) return;
    ensureBackendPointer();

    toneSequence[0] = f1;
    toneSequence[1] = dur1024_to_ticks(d1);
    toneSequence[2] = f2;
    toneSequence[3] = dur1024_to_ticks(d2);
    toneSequence[4] = TONES_END;

    tonesBackend->tonesInRAM(toneSequence);
}

void ArduboyTonesFX::tone(
    uint16_t f1,
    uint16_t d1,
    uint16_t f2,
    uint16_t d2,
    uint16_t f3,
    uint16_t d3) {
    if(!isOutputEnabledNow()) return;
    ensureBackendPointer();

    toneSequence[0] = f1;
    toneSequence[1] = dur1024_to_ticks(d1);
    toneSequence[2] = f2;
    toneSequence[3] = dur1024_to_ticks(d2);
    toneSequence[4] = f3;
    toneSequence[5] = dur1024_to_ticks(d3);
    toneSequence[6] = TONES_END;

    tonesBackend->tonesInRAM(toneSequence);
}

uint16_t ArduboyTonesFX::copyPattern(const uint16_t* src, bool progmem) {
    uint16_t out = 0;

    while(out + 1 < ARDUBOY_TONESFX_MAX_WORDS) {
        uint16_t w = progmem ? pgm_read_word(src++) : *src++;
        seqBuffer[out++] = w;

        if(w == TONES_END) break;
        if(w == TONES_REPEAT) continue;

        uint16_t d = progmem ? pgm_read_word(src++) : *src++;
        seqBuffer[out++] = dur1024_to_ticks(d);
    }

    if(out == 0 || seqBuffer[out - 1] != TONES_END) {
        seqBuffer[(out < ARDUBOY_TONESFX_MAX_WORDS) ? out : (ARDUBOY_TONESFX_MAX_WORDS - 1)] =
            TONES_END;
    }

    return out;
}

void ArduboyTonesFX::tones(const uint16_t* pattern) {
    if(!isOutputEnabledNow()) return;
    if(!pattern) return;
    ensureBackendPointer();

    copyPattern(pattern, true);
    tonesBackend->tonesInRAM(seqBuffer);
}

void ArduboyTonesFX::tonesInRAM(uint16_t* pattern) {
    if(!isOutputEnabledNow()) return;
    if(!pattern) return;
    ensureBackendPointer();

    copyPattern(pattern, false);
    tonesBackend->tonesInRAM(seqBuffer);
}

uint16_t ArduboyTonesFX::decodeFx(uint32_t fx_addr) {
    uint16_t out = 0;
    FX::seekData(fx_addr);

    while(out + 1 < ARDUBOY_TONESFX_MAX_WORDS) {
        uint16_t t = FX::readPendingUInt8();
        t |= (uint16_t)FX::readPendingUInt8() << 8;
        seqBuffer[out++] = t;

        if(t == TONES_END) break;
        if(t == TONES_REPEAT) continue;

        uint16_t d = FX::readPendingUInt8();
        d |= (uint16_t)FX::readPendingUInt8() << 8;
        seqBuffer[out++] = dur1024_to_ticks(d);
    }

    FX::readEnd();

    if(out == 0 || seqBuffer[out - 1] != TONES_END) {
        seqBuffer[(out < ARDUBOY_TONESFX_MAX_WORDS) ? out : (ARDUBOY_TONESFX_MAX_WORDS - 1)] =
            TONES_END;
    }

    return out;
}

void ArduboyTonesFX::tonesFromFX(uint32_t fx_addr) {
    if(!isOutputEnabledNow()) return;
    ensureBackendPointer();

    decodeFx(fx_addr);
    tonesBackend->tonesInRAM(seqBuffer);
}

void ArduboyTonesFX::noTone() {
    if(tonesBackend) tonesBackend->noTone();
}

void ArduboyTonesFX::volumeMode(uint8_t mode) {
    ArduboyTones::volumeMode(mode);
}

bool ArduboyTonesFX::playing() {
    return ArduboyTones::playing();
}
