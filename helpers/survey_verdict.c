#include "survey_verdict.h"

/* A carrier up for at least this share of the survey is a reader doing its job,
 * not a stray blip. */
#define ACTIVE_IN_FIELD_PCT 20u

/* ...or a single reading this strong, which only happens with an emitter close
 * enough that duration stops mattering.
 *
 * This is a *meter* reading (see field_scale.h), not a raw duty-cycle. That
 * distinction used to be a bug: peaks were fed in as raw duty, which saturates
 * near 30% on a normal polling reader, so this test could essentially never
 * fire and ACTIVE rested on the in-field criterion alone. On the scaled meter a
 * reader you are right on top of reads ~90-100, so a peak of 50 now means what
 * it was always meant to mean - "briefly, something was unmistakably close". */
#define ACTIVE_PEAK 50u

uint8_t survey_in_field_pct(const SurveySummary* s) {
    if(!s || s->elapsed_ms == 0) return 0;
    uint32_t pct = ((uint64_t)s->in_field_ms * 100u) / s->elapsed_ms;
    return (uint8_t)(pct > 100u ? 100u : pct);
}

SurveyVerdict survey_verdict(const SurveySummary* s) {
    if(!s) return SurveyVerdictClean;

    /* No contact at all is the only route to CLEAN. Note this is "clean at the
     * sensitivity you chose" - the caller's threshold defines the floor, and a
     * dormant or shielded reader stays invisible to any of them. */
    if(s->contacts == 0) return SurveyVerdictClean;

    if(survey_in_field_pct(s) >= ACTIVE_IN_FIELD_PCT || s->peak >= ACTIVE_PEAK) {
        return SurveyVerdictActive;
    }

    return SurveyVerdictTrace;
}

const char* survey_verdict_name(SurveyVerdict v) {
    switch(v) {
    case SurveyVerdictActive:
        return "ACTIVE READER";
    case SurveyVerdictTrace:
        return "TRACE";
    case SurveyVerdictClean:
    default:
        return "CLEAN";
    }
}

const char* survey_verdict_advice(SurveyVerdict v) {
    switch(v) {
    case SurveyVerdictActive:
        return "Fingerprint it";
    case SurveyVerdictTrace:
        return "Sweep again, slower";
    case SurveyVerdictClean:
    default:
        return "Nothing emitting here";
    }
}
