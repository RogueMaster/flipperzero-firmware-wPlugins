#pragma once

#include <stdint.h>

#include <lib/ArduboyFX.h>
#include <lib/ArduboyTones.h>

class ArduboyTonesFX {
public:
    ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, uint8_t tonesArrayLen);

    template <size_t size>
    ArduboyTonesFX(bool (*outEn)(), uint16_t (&buffer)[size])
        : ArduboyTonesFX(outEn, buffer, (uint8_t)size) {
        static_assert(size < 256, "Buffer too large");
    }

    static void tonesFromFX(uint24_t tones);
    static void fillBufferFromFX();
    static void noTone();
    static void volumeMode(uint8_t mode);
    static bool playing();

private:
    static constexpr size_t MaxWords = 512;

    static ArduboyTones backend_;
    static uint16_t sequence_[MaxWords];
    static bool (*outputEnabled_)();

    static size_t decodeFromFX_(uint24_t addr);
};
