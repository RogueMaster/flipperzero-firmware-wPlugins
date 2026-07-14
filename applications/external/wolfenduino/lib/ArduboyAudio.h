#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "ArduboyTones.h"

class ArduboyAudio {
public:
    void begin() {
        const bool system_enabled = systemAudioEnabled();

        if(system_enabled) {
            on();
        } else {
            off();
        }
    }

    void on() {
        bool was_enabled = g_arduboy_audio_enabled;
        g_arduboy_audio_enabled = true;
        if(!was_enabled) {
            arduboy_tone_sound_system_init();
        }
    }

    void off() {
        bool was_enabled = g_arduboy_audio_enabled;
        g_arduboy_audio_enabled = false;
        if(was_enabled) {
            ArduboyToneSoundRequest req = {.pattern = NULL};
            if(g_arduboy_sound_queue) (void)furi_message_queue_put(g_arduboy_sound_queue, &req, 0);
            arduboy_tone_sound_system_deinit();
        }
    }

    static bool enabled() {
        return g_arduboy_audio_enabled;
    }

    void toggle() {
        if(enabled()) {
            off();
        } else {
            on();
        }
    }

    bool systemAudioEnabled() const {
        // Keep compatibility with public SDK headers used by ufbt/CI:
        // notification settings internals are not part of public API.
        return !furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
    }

    void saveOnOff() {
        // Audio state is driven by current OS/session settings.
    }
};
