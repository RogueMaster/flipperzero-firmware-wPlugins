/* What the LED, the vibro motor and the speaker do for a feedback cue of
 * the reducer (AppState.cue), per the Feedback and Sound settings:
 *
 *   connected, read   blue + short vibro
 *   success           green + short vibro (+ success beeps)
 *   error             red + long vibro (+ error beeps)
 *
 * Feedback = LED drops the vibro, Off drops the LED and the vibro; Sound =
 * On adds the beeps to success and error whatever Feedback says.
 * feedback_play.c turns a plan into notification sequences.
 */
#pragma once

#include <stdbool.h>

#include "app_settings.h"
#include "app_state.h"

typedef enum {
    FeedbackLedNone,
    FeedbackLedBlue,
    FeedbackLedGreen,
    FeedbackLedRed,
} FeedbackLed;

typedef enum {
    FeedbackVibroNone,
    FeedbackVibroShort,
    FeedbackVibroLong,
} FeedbackVibro;

typedef enum {
    FeedbackSoundNone,
    FeedbackSoundSuccess,
    FeedbackSoundError,
} FeedbackSound;

typedef struct {
    FeedbackLed led;
    FeedbackVibro vibro;
    FeedbackSound sound;
} FeedbackPlan;

FeedbackPlan feedback_plan(AppCue cue, const AppSettings* settings);

bool feedback_plan_empty(FeedbackPlan plan);
