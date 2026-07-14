#include "ArduboyTonesFX.h"

ArduboyTones ArduboyTonesFX::backend_ = ArduboyTones(false);
uint16_t ArduboyTonesFX::sequence_[ArduboyTonesFX::MaxWords] = {0};
bool (*ArduboyTonesFX::outputEnabled_)() = nullptr;

ArduboyTonesFX::ArduboyTonesFX(bool (*outEn)(), uint16_t* tonesArray, uint8_t tonesArrayLen) {
    UNUSED(tonesArray);
    UNUSED(tonesArrayLen);
    outputEnabled_ = outEn;
}

size_t ArduboyTonesFX::decodeFromFX_(uint24_t addr) {
    size_t i = 0;

    FX::seekData(addr);

    while(i + 1 < MaxWords) {
        const uint16_t freq = FX::readPendingUInt16();
        sequence_[i++] = freq;

        if(freq == TONES_END || freq == TONES_REPEAT) {
            break;
        }

        const uint16_t dur = FX::readPendingUInt16();
        sequence_[i++] = dur;
    }

    if(i == 0 || (sequence_[i - 1] != TONES_END && sequence_[i - 1] != TONES_REPEAT)) {
        if(i < MaxWords) {
            sequence_[i++] = TONES_END;
        } else {
            sequence_[MaxWords - 1] = TONES_END;
        }
    }

    FX::readEnd();
    return i;
}

void ArduboyTonesFX::tonesFromFX(uint24_t tones) {
    if(outputEnabled_ && !outputEnabled_()) return;

    decodeFromFX_(tones);
    backend_.tonesInRAM(sequence_);
}

void ArduboyTonesFX::fillBufferFromFX() {
}

void ArduboyTonesFX::noTone() {
    backend_.noTone();
}

void ArduboyTonesFX::volumeMode(uint8_t mode) {
    ArduboyTones::volumeMode(mode);
}

bool ArduboyTonesFX::playing() {
    return ArduboyTones::playing();
}
