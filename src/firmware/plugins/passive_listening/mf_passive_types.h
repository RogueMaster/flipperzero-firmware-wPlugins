#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <input/input.h>

#include "../common/mf_callsign_gen.h"

#define MF_PASSIVE_PCM_RING_SAMPLES 1024U

typedef enum {
    MfPassiveOutputInternal = 0,
    MfPassiveOutputP2 = 1,
} MfPassiveOutputTarget;

typedef struct MfPassivePcmPipe {
    int16_t samples[MF_PASSIVE_PCM_RING_SAMPLES];
    volatile uint16_t read_pos;
    volatile uint16_t write_pos;
    volatile bool eof;
    volatile bool drained;
    volatile uint32_t underruns;
} MfPassivePcmPipe;

typedef struct {
    uint32_t struct_size;
    void* context;
    bool (*claim)(void* context, MfPassiveOutputTarget target, uint8_t volume_pct, MfPassivePcmPipe* pipe);
    bool (*set_silence)(void* context);
    bool (*set_tone)(void* context, uint16_t tone_hz);
    bool (*set_voice)(void* context, uint32_t source_rate_hz);
    void (*set_vibration)(void* context, bool enabled);
    void (*release)(void* context);
} MfPassiveHostServices;

typedef struct {
    uint32_t struct_size;
    uint32_t now_ms;
    uint32_t rng_seed;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    uint16_t tone_hz;
    uint8_t output_target;
    uint8_t volume_pct;
    const MfPassiveHostServices* services;
} MfPassiveEnterArgs;

typedef enum {
    MfPassivePhasePrepare = 0,
    MfPassivePhaseCw,
    MfPassivePhasePostCw,
    MfPassivePhaseVoicePrime,
    MfPassivePhaseVoice,
    MfPassivePhaseBetweenTokens,
    MfPassivePhasePostVoice,
    MfPassivePhaseCue,
    MfPassivePhasePostCue,
    MfPassivePhaseError,
} MfPassivePhase;

typedef struct {
    bool handled;
    bool redraw;
    bool request_exit;
    uint8_t phase;
    uint8_t error;
} MfPassiveResult;
