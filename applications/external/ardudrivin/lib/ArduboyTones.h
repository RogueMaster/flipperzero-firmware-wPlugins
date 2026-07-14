// lib/ArduboyTones.h
#pragma once
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>

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
#define ARDUBOY_TONES_TICK_HZ 780u
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

typedef struct {
    const uint16_t* pattern;
} ArduboyToneSoundRequest;

extern FuriMessageQueue* g_arduboy_sound_queue;
extern FuriThread* g_arduboy_sound_thread;
extern volatile bool g_arduboy_sound_thread_running;
extern volatile bool g_arduboy_audio_enabled;
extern volatile bool g_arduboy_tones_playing;
extern volatile uint8_t g_arduboy_volume_mode;
extern volatile bool g_arduboy_force_high;
extern volatile bool g_arduboy_force_norm;

static constexpr float kArduboyToneSoundVolumeNormal = 1.0f;
static constexpr float kArduboyToneSoundVolumeHigh = 1.0f;
static constexpr uint32_t kArduboyToneToneTickHz = ARDUBOY_TONES_TICK_HZ;

static inline uint32_t arduboy_tone_ticks_to_ms(uint16_t ticks) {
    return (uint32_t)((ticks * 1000u + (kArduboyToneToneTickHz / 2)) / kArduboyToneToneTickHz);
}

static inline float arduboy_tone_volume_for(uint16_t freq_word) {
    bool want_high = false;
    if(g_arduboy_force_norm)
        want_high = false;
    else if(g_arduboy_force_high)
        want_high = true;
    else
        want_high = ((freq_word & TONE_HIGH_VOLUME) != 0);
    return want_high ? kArduboyToneSoundVolumeHigh : kArduboyToneSoundVolumeNormal;
}

static inline uint16_t arduboy_tone_strip_volume(uint16_t freq_word) {
    return (uint16_t)(freq_word & (uint16_t)~TONE_HIGH_VOLUME);
}

static int32_t arduboy_tone_sound_thread_fn(void* /*ctx*/) {
    ArduboyToneSoundRequest req;
    const uint16_t* current_pattern = NULL;

    while(g_arduboy_sound_thread_running) {
        if(furi_message_queue_get(g_arduboy_sound_queue, &req, 50) != FuriStatusOk) {
            continue;
        }

        if(!req.pattern) {
            current_pattern = NULL;
            if(furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
            g_arduboy_tones_playing = false;
            continue;
        }

        current_pattern = req.pattern;

        if(!g_arduboy_audio_enabled) {
            g_arduboy_tones_playing = false;
            continue;
        }

        if(!furi_hal_speaker_acquire(50)) {
            g_arduboy_tones_playing = false;
            continue;
        }

        g_arduboy_tones_playing = true;

        const uint16_t* start = current_pattern;
        const uint16_t* p = current_pattern;

        while(g_arduboy_sound_thread_running && g_arduboy_audio_enabled) {
            ArduboyToneSoundRequest new_req;
            if(furi_message_queue_get(g_arduboy_sound_queue, &new_req, 0) == FuriStatusOk) {
                if(!new_req.pattern) {
                    g_arduboy_tones_playing = false;
                    break;
                } else {
                    current_pattern = new_req.pattern;
                    start = current_pattern;
                    p = current_pattern;
                }
            }

            uint16_t freq_word = *p++;

            if(freq_word == TONES_END) {
                g_arduboy_tones_playing = false;
                break;
            }

            if(freq_word == TONES_REPEAT) {
                p = start;
                continue;
            }

            uint16_t dur_ticks = *p++;

            float vol = arduboy_tone_volume_for(freq_word);
            uint16_t freq = arduboy_tone_strip_volume(freq_word);

            if(dur_ticks == 0) {
                if(freq == 0) {
                    furi_hal_speaker_stop();
                } else {
                    furi_hal_speaker_start((float)freq, vol);
                }

                while(g_arduboy_sound_thread_running && g_arduboy_audio_enabled) {
                    ArduboyToneSoundRequest nr;
                    if(furi_message_queue_get(g_arduboy_sound_queue, &nr, 50) == FuriStatusOk) {
                        if(!nr.pattern) {
                            g_arduboy_tones_playing = false;
                        } else {
                            current_pattern = nr.pattern;
                            start = current_pattern;
                            p = current_pattern;
                        }
                        break;
                    }
                }

                furi_hal_speaker_stop();
                if(!g_arduboy_tones_playing) break;
                continue;
            }

            uint32_t dur_ms = arduboy_tone_ticks_to_ms(dur_ticks);
            if(dur_ms == 0) dur_ms = 1;

            if(freq == 0) {
                furi_hal_speaker_stop();
                furi_delay_ms(dur_ms);
            } else {
                furi_hal_speaker_start((float)freq, vol);
                furi_delay_ms(dur_ms);
                furi_hal_speaker_stop();
            }
        }

        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    g_arduboy_tones_playing = false;
    return 0;
}

static void arduboy_tone_sound_system_init() {
    if(g_arduboy_sound_queue || g_arduboy_sound_thread) return;
    g_arduboy_sound_queue = furi_message_queue_alloc(4, sizeof(ArduboyToneSoundRequest));
    g_arduboy_sound_thread = furi_thread_alloc();
    furi_thread_set_name(g_arduboy_sound_thread, "ArduboySound");
    furi_thread_set_stack_size(g_arduboy_sound_thread, 1024);
    furi_thread_set_priority(g_arduboy_sound_thread, FuriThreadPriorityNormal);
    furi_thread_set_callback(g_arduboy_sound_thread, arduboy_tone_sound_thread_fn);
    g_arduboy_sound_thread_running = true;
    furi_thread_start(g_arduboy_sound_thread);
}

static void arduboy_tone_sound_system_deinit() {
    if(!g_arduboy_sound_thread) return;
    g_arduboy_sound_thread_running = false;
    furi_thread_join(g_arduboy_sound_thread);
    furi_thread_free(g_arduboy_sound_thread);
    g_arduboy_sound_thread = NULL;
    if(g_arduboy_sound_queue) {
        furi_message_queue_free(g_arduboy_sound_queue);
        g_arduboy_sound_queue = NULL;
    }
    g_arduboy_tones_playing = false;
}

class ArduboyAudio;
class ArduboyTones {
public:
    explicit ArduboyTones(bool /*enabled*/) {}
    ArduboyTones() {}

    void attachAudio(ArduboyAudio* /*audio*/) {}

    void begin() {
        g_arduboy_audio_enabled = true;
        arduboy_tone_sound_system_init();
    }

    static void volumeMode(uint8_t mode) {
        g_arduboy_volume_mode = mode;
        g_arduboy_force_high = false;
        g_arduboy_force_norm = false;
        if(mode == VOLUME_ALWAYS_NORMAL)
            g_arduboy_force_norm = true;
        else if(mode == VOLUME_ALWAYS_HIGH)
            g_arduboy_force_high = true;
    }

    static bool playing() {
        return g_arduboy_tones_playing;
    }

    void tones(const uint16_t* pattern) {
        if(!g_arduboy_audio_enabled) return;
        if(!pattern) return;
        if(!g_arduboy_sound_queue) return;

        ArduboyToneSoundRequest req = {.pattern = pattern};
        if(furi_message_queue_put(g_arduboy_sound_queue, &req, 0) != FuriStatusOk) {
            ArduboyToneSoundRequest dummy;
            (void)furi_message_queue_get(g_arduboy_sound_queue, &dummy, 0);
            (void)furi_message_queue_put(g_arduboy_sound_queue, &req, 0);
        }
        g_arduboy_tones_playing = true;
    }

    void tonesInRAM(uint16_t* pattern) {
        tones((const uint16_t*)pattern);
    }

    void noTone() {
        if(!g_arduboy_sound_queue) {
            g_arduboy_tones_playing = false;
            return;
        }
        ArduboyToneSoundRequest req = {.pattern = NULL};
        (void)furi_message_queue_put(g_arduboy_sound_queue, &req, 0);
        g_arduboy_tones_playing = false;
    }

    void tone(uint16_t frequency, uint16_t duration_ms) {
        if(!g_arduboy_audio_enabled) return;

        uint32_t ticks;
        if(duration_ms == 0) {
            ticks = 0;
        } else {
            ticks = (duration_ms * kArduboyToneToneTickHz + 500u) / 1000u;
            if(ticks == 0) ticks = 1;
            if(ticks > 0xFFFFu) ticks = 0xFFFFu;
        }

        uint8_t i = inline_idx_;
        inline_idx_ = (uint8_t)((inline_idx_ + 1u) % 8u);

        inline_patterns_[i][0] = frequency;
        inline_patterns_[i][1] = (uint16_t)ticks;
        inline_patterns_[i][2] = TONES_END;

        tones(inline_patterns_[i]);
    }

    void tone(uint16_t freq, uint16_t dur_ms, uint16_t freq2, uint16_t dur2_ms) {
        if(!g_arduboy_audio_enabled) return;

        uint16_t t1 = ms_to_ticks16_(dur_ms);
        uint16_t t2 = ms_to_ticks16_(dur2_ms);

        uint8_t i = inline_idx2_;
        inline_idx2_ = (uint8_t)((inline_idx2_ + 1u) % 8u);

        inline_patterns2_[i][0] = freq;
        inline_patterns2_[i][1] = t1;
        inline_patterns2_[i][2] = freq2;
        inline_patterns2_[i][3] = t2;
        inline_patterns2_[i][4] = TONES_END;

        tones(inline_patterns2_[i]);
    }

    void tone(uint16_t f1, uint16_t d1_ms, uint16_t f2, uint16_t d2_ms, uint16_t f3, uint16_t d3_ms) {
        if(!g_arduboy_audio_enabled) return;

        uint16_t t1 = ms_to_ticks16_(d1_ms);
        uint16_t t2 = ms_to_ticks16_(d2_ms);
        uint16_t t3 = ms_to_ticks16_(d3_ms);

        uint8_t i = inline_idx3_;
        inline_idx3_ = (uint8_t)((inline_idx3_ + 1u) % 8u);

        inline_patterns3_[i][0] = f1;
        inline_patterns3_[i][1] = t1;
        inline_patterns3_[i][2] = f2;
        inline_patterns3_[i][3] = t2;
        inline_patterns3_[i][4] = f3;
        inline_patterns3_[i][5] = t3;
        inline_patterns3_[i][6] = TONES_END;

        tones(inline_patterns3_[i]);
    }

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
