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
     * dormant or shielded reader stays invisible to any of them.
     *
     * ...and only if the survey ran long enough for "nothing" to mean anything.
     * A run cut short before SPECTER_SURVEY_MIN_CLEAN_MS says TOO SHORT rather
     * than CLEAN. The asymmetry is deliberate and is the same rule the rest of
     * the app follows: a positive finding stands on its own evidence however
     * brief, while a negative one is a claim about the whole room and has to be
     * earned with time. TRACE and ACTIVE are therefore never downgraded. */
    if(s->contacts == 0) {
        return s->elapsed_ms < SPECTER_SURVEY_MIN_CLEAN_MS ? SurveyVerdictTooShort :
                                                             SurveyVerdictClean;
    }

    /* peak_ref, not peak: the Meter setting is a display preference. Judged on
     * the displayed peak, switching Meter to Raw put this threshold back out of
     * reach for a polling reader - exactly the bug the comment above says was
     * fixed - so a brief close pass over a skimmer was filed as TRACE while the
     * identical survey on Boost reported ACTIVE. */
    if(survey_in_field_pct(s) >= ACTIVE_IN_FIELD_PCT || s->peak_ref >= ACTIVE_PEAK) {
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
    case SurveyVerdictTooShort:
        return "TOO SHORT";
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
    case SurveyVerdictTooShort:
        return "Let it run longer";
    case SurveyVerdictClean:
    default:
        /* Not "No field detected". CLEAN means nothing crossed the sensitivity
         * threshold - but PEAK and AVG are raised from every sample, gated by
         * nothing, so an ordinary room's noise prints a few percent right
         * above this line. A card reading "PEAK 9% / No field detected" is a
         * screen arguing with itself, and the number is the honest half: a
         * field WAS measured, it just was not a reader. Say what was actually
         * decided. */
        return "Nothing above floor";
    }
}
