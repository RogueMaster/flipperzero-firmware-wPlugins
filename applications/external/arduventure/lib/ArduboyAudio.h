#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "include/ArduboyAudioState.h"
#ifdef ARDULIB_USE_TONES
#include "ArduboyTones.h"
#endif
#ifdef ARDULIB_USE_ATM
#include "ATMlib.h"
#endif

class ArduboyAudio {
public:
    void begin() {
        if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
            off();
        } else {
            on();
        }
    }

    void on() {
        g_arduboy_audio_enabled = true;
#ifdef ARDULIB_USE_TONES
        ardulib_tone_init();
#endif
#ifdef ARDULIB_USE_ATM
        ATMsynth::systemInit();
        ATMsynth::setEnabled(true);
#endif
    }

    void off() {
        g_arduboy_audio_enabled = false;
#ifdef ARDULIB_USE_TONES
        ardulib_tone_stop();
        ardulib_tone_deinit();
#endif
#ifdef ARDULIB_USE_ATM
        ATMsynth::setEnabled(false);
        ATMsynth::systemDeinit();
#endif
    }

    static bool enabled() {
        return g_arduboy_audio_enabled;
    }

    void toggle() {
        if(g_arduboy_audio_enabled) {
            off();
        } else {
            on();
        }
    }

    void saveOnOff() {
    }
};
