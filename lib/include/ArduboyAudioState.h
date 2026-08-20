#pragma once

#include <stdbool.h>
#include <stdint.h>
#ifdef ARDULIB_USE_TONES
#include <furi.h>
#endif

#ifdef ARDULIB_USE_TONES
typedef struct {
    const uint16_t* pattern;
} ArduboyToneSoundRequest;

extern FuriMessageQueue* g_arduboy_sound_queue;
extern FuriThread* g_arduboy_sound_thread;
extern volatile bool g_arduboy_sound_thread_running;
#endif

extern volatile bool g_arduboy_audio_enabled;

#ifdef ARDULIB_USE_TONES
extern volatile bool g_arduboy_tones_playing;
extern volatile uint8_t g_arduboy_volume_mode;
extern volatile bool g_arduboy_force_high;
extern volatile bool g_arduboy_force_norm;
#endif
