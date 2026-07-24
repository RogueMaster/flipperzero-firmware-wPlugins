#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MF_RX_PRACTICE_FEED_MAX 96U

typedef enum { MfRxPracticeModeCallsigns = 0, MfRxPracticeModeGroups5 = 1 } MfRxPracticeMode;
typedef enum {
    MfRxPracticePhaseIdle = 0,
    MfRxPracticePhasePlayback,
    MfRxPracticePhaseAnswer,
    MfRxPracticePhaseResult,
    MfRxPracticePhaseFinal,
} MfRxPracticePhase;
typedef enum {
    MfRxPracticeCommandNone = 0,
    MfRxPracticeCommandStart,
    MfRxPracticeCommandBackspace,
    MfRxPracticeCommandClear,
    MfRxPracticeCommandHurry,
    MfRxPracticeCommandBack,
    MfRxPracticeCommandConfirmExit,
} MfRxPracticeCommand;
typedef enum {
    MfRxPracticeFeedbackNone = 0,
    MfRxPracticeFeedbackClear,
    MfRxPracticeFeedbackPass,
    MfRxPracticeFeedbackFail,
    MfRxPracticeFeedbackTimeout,
} MfRxPracticeFeedback;

typedef struct {
    uint32_t struct_size;
    MfRxPracticeMode mode;
    uint32_t now_ms;
    uint32_t rng_seed;
    uint32_t answer_timeout_ms;
    uint32_t result_hold_ms;
    uint16_t dit_ms;
    uint16_t char_gap_ms;
    bool physical_key_can_start;
} MfRxPracticeEnterArgs;

typedef struct {
    bool handled;
    bool redraw;
    bool decoder_reset;
    bool request_exit;
    MfRxPracticePhase phase;
    bool playback_mark;
    MfRxPracticeFeedback feedback;
} MfRxPracticeResult;
