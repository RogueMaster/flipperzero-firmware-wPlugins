#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Site survey: the verdict layer.
 *
 * A sweep answers "is there a reader right here, right now". A survey answers
 * the question you actually walked into the room with: "over the last minute of
 * me moving around, was anything emitting?".
 *
 * Deliberately three outcomes, not a letter grade. A grade would imply a
 * calibrated scale for how compromised a room is, and no such scale exists here
 * - Specter measures a carrier's duty-cycle, not a threat level. Three plain
 * outcomes with an explicit "what to do next" is the honest shape.
 *
 * Pure and host-tested in test/. */

typedef enum {
    SurveyVerdictClean = 0, // nothing crossed the noise floor
    SurveyVerdictTrace, // brief or faint hits - worth a second pass
    SurveyVerdictActive, // a reader was up and emitting for real
} SurveyVerdict;

typedef struct {
    uint32_t elapsed_ms; // survey wall time
    uint32_t in_field_ms; // of which, carrier present
    uint8_t peak; // 0..100 strongest reading
    uint8_t average; // 0..100 mean reading
    uint32_t contacts; // distinct appearances
} SurveySummary;

/* Percentage of the survey during which a carrier was up (0 if no time ran). */
uint8_t survey_in_field_pct(const SurveySummary* s);

SurveyVerdict survey_verdict(const SurveySummary* s);

const char* survey_verdict_name(SurveyVerdict v); // "ACTIVE READER"
const char* survey_verdict_advice(SurveyVerdict v); // one-line next step
