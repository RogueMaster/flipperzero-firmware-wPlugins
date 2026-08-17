#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <input/input.h>

#include "../common/mf_callsign_gen.h"
#include "mf_passive_audio_pipe.h"

#define MF_PASSIVE_LESSON_CHARSET_CAP 40U

typedef struct VariableItemList VariableItemList;
typedef struct VariableItem VariableItem;

typedef enum {
    MfPassiveEntryPlayback = 0,
    MfPassiveEntrySettings = 1,
} MfPassiveEntryKind;

typedef struct {
    uint8_t mode;
    uint8_t length;
    uint8_t lesson;
    uint16_t dit_ms;
    uint8_t farnsworth_wpm;
    uint8_t vibrate;
    uint8_t answer_delay_s;
    uint8_t repeat_after_answer;
    uint8_t courtesy_delay_half_s;
    uint8_t selected_row;
    uint8_t transmit_fm;
} MfPassiveSettingsModel;

typedef struct {
    VariableItemList* list;
} MfPassiveSettingsArgs;

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
    bool (*command)(
        void* context,
        MfPassiveHostCommand command,
        uint32_t value,
        MfPassivePcmPipe* pipe);
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
    uint32_t now_ms;
    uint32_t rng_seed;
    uint32_t frequency_hz;
    uint16_t tone_hz;
    uint8_t output_target;
    uint8_t volume_pct;
    const MfPassiveHostServices* services;
} MfPassivePlaybackArgs;

typedef union {
    MfPassivePlaybackArgs playback;
    MfPassiveSettingsArgs settings;
} MfPassiveEntryArgs;

typedef struct {
    uint32_t struct_size;
    uint8_t entry_kind;
    MfPassiveEntryArgs entry;
} MfPassiveEnterArgs;

typedef enum {
    MfPassivePhaseLoading = 0,
    MfPassivePhasePrepare,
    MfPassivePhaseInitialRfLock,
    MfPassivePhaseCw,
    MfPassivePhasePostCw,
    MfPassivePhaseVoicePrime,
    MfPassivePhaseVoiceRfLock,
    MfPassivePhaseVoice,
    MfPassivePhaseBetweenTokens,
    MfPassivePhasePostVoice,
    MfPassivePhaseRepeatRfLock,
    MfPassivePhaseCueRfLock,
    MfPassivePhaseNextRfLock,
    MfPassivePhaseCue,
    MfPassivePhasePostCue,
    MfPassivePhaseRepeatCw,
    MfPassivePhaseError,
    MfPassivePhasePostRepeat,
} MfPassivePhase;

typedef enum {
    MfPassiveErrorNone = 0,
    MfPassiveErrorAudio,
    MfPassiveErrorFmUnavailable,
} MfPassiveError;

typedef struct {
    bool handled;
    bool redraw;
    bool request_exit;
    uint8_t phase;
    uint8_t error;
    uint8_t feedback;
} MfPassiveResult;

enum {
    MfPassiveFeedbackNone = 0,
    MfPassiveFeedbackRoundComplete = 1,
};
