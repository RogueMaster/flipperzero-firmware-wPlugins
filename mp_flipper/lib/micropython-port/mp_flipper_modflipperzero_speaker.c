#include <furi_hal.h>

#include <mp_flipper_modflipperzero.h>

inline bool mp_flipper_speaker_start(float frequency, float volume) {
    if(furi_hal_speaker_acquire(100)) {
        furi_hal_speaker_start(frequency, volume);

        return true;
    }

    return false;
}

inline bool mp_flipper_speaker_set_volume(float volume) {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_set_volume(volume);

        return true;
    }

    return false;
}

inline bool mp_flipper_speaker_stop() {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();

        furi_hal_speaker_release();

        return true;
    }

    return false;
}
