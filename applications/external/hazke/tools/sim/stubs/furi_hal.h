#pragma once
#include <stdint.h>
inline bool furi_hal_speaker_acquire(uint32_t) {
    return false;
}
inline void furi_hal_speaker_release() {
}
inline void furi_hal_speaker_start(float, float) {
}
inline void furi_hal_speaker_stop() {
}
inline void furi_hal_vibro_on(bool) {
}
