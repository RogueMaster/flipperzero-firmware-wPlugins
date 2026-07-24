#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../morse_flipper_mapped_fal.h"

#define MF_RX_PRACTICE_FEED_MAX 96U

typedef uint8_t MfRxPracticePhase;
enum {
    MfRxPracticePhaseIdle = 0,
    MfRxPracticePhasePlayback,
    MfRxPracticePhaseAnswer,
    MfRxPracticePhaseResult,
    MfRxPracticePhaseFinal,
};
typedef uint8_t MfRxPracticeCommand;
enum {
    MfRxPracticeCommandNone = 0,
    MfRxPracticeCommandStart,
    MfRxPracticeCommandBackspace,
    MfRxPracticeCommandClear,
    MfRxPracticeCommandHurry,
    MfRxPracticeCommandBack,
    MfRxPracticeCommandConfirmExit,
    MfRxPracticeCommandPrimaryPress,
    MfRxPracticeCommandPaddleBackPress,
    MfRxPracticeCommandReleaseOk,
    MfRxPracticeCommandReleaseBack,
};
typedef uint8_t MfRxPracticeFeedback;
enum {
    MfRxPracticeFeedbackNone = 0,
    MfRxPracticeFeedbackClear,
    MfRxPracticeFeedbackPass,
    MfRxPracticeFeedbackFail,
    MfRxPracticeFeedbackTimeout,
};

typedef struct {
    uint32_t struct_size;
    uint32_t now_ms;
    uint32_t rng_seed;
    uint32_t answer_timeout_ms;
    uint32_t result_hold_ms;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    bool physical_key_can_start;
} MfRxPracticeEnterArgs;

typedef MorseFlipperMappedFalResult MfRxPracticeResult;
