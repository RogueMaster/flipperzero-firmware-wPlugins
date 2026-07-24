#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <input/input.h>

#include "../common/mf_callsign_gen.h"
#include "mf_passive_audio_pipe.h"

#define MF_PASSIVE_LESSON_CHARSET_CAP 40U

typedef enum {
    MfPassiveOutputInternal = 0,
    MfPassiveOutputP2 = 1,
} MfPassiveOutputTarget;

typedef enum {
    MfPassiveHostCommandClaim = 0,
    MfPassiveHostCommandSilence,
    MfPassiveHostCommandTone,
    MfPassiveHostCommandVoice,
    MfPassiveHostCommandVibration,
    MfPassiveHostCommandRelease,
} MfPassiveHostCommand;

typedef struct {
    uint32_t struct_size;
    void* context;
    bool (*command)(void* context, MfPassiveHostCommand command, uint32_t value, MfPassivePcmPipe* pipe);
} MfPassiveHostServices;

static inline bool mf_passive_host_claim(
    const MfPassiveHostServices* services,
    MfPassiveOutputTarget target,
    uint16_t tone_hz,
    uint8_t volume_pct,
    MfPassivePcmPipe* pipe) {
    return services != NULL && services->command != NULL &&
           services->command(
               services->context,
               MfPassiveHostCommandClaim,
               ((uint32_t)tone_hz << 16U) | ((uint32_t)target << 8U) | volume_pct,
               pipe);
}

static inline bool mf_passive_host_command(
    const MfPassiveHostServices* services,
    MfPassiveHostCommand command,
    uint32_t value) {
    return services != NULL && services->command != NULL &&
           services->command(services->context, command, value, NULL);
}

typedef struct {
    uint32_t struct_size;
    uint32_t now_ms;
    uint32_t rng_seed;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    uint16_t tone_hz;
    uint16_t answer_delay_ms;
    uint8_t output_target;
    uint8_t volume_pct;
    uint8_t mode;
    uint8_t prompt_length;
    uint8_t lesson_charset_len;
    uint8_t vibrate;
    uint8_t repeat_after_answer;
    char lesson_charset[MF_PASSIVE_LESSON_CHARSET_CAP];
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
