#include "emitter_classify.h"

/* A carrier this close to permanently-on is treated as continuous wave: the
 * gaps left are shorter than anything we could honestly resolve anyway. */
#define CONTINUOUS_DUTY 95u

/* Jitter, as a percentage of the period, under which a cadence counts as
 * "fixed". A reader's polling loop is driven by a crystal-timed state machine,
 * so a genuine poll is far steadier than this; hand movement and RF noise are
 * not. */
#define POLLING_JITTER_PCT 15u

EmitterVerdict emitter_classify(const CadenceStats* c) {
    EmitterVerdict v = {EmitterClassNoField, 0, false};
    if(!c) return v;

    /* Silence. This is the one verdict we can give with total confidence -
     * within the limits of the noise floor the caller chose. */
    if(c->duty == 0 && c->bursts == 0) {
        v.klass = EmitterClassNoField;
        v.confidence = 100;
        v.timing_reliable = true;
        return v;
    }

    /* Durations only mean something when they span several samples. Anything
     * tighter than that is the sampler's own granularity showing through, and
     * we say so rather than quoting a number that looks precise. */
    v.timing_reliable = (c->period_ms >= SPECTER_MIN_PERIOD_SAMPLES * SPECTER_SAMPLE_MS) &&
                        (c->burst_ms >= 2u * SPECTER_SAMPLE_MS);

    /* Unbroken carrier. */
    if(c->duty >= CONTINUOUS_DUTY) {
        uint8_t d = c->duty > 100u ? 100u : c->duty;
        v.klass = EmitterClassContinuous;
        v.confidence = (uint8_t)(60u + (d - CONTINUOUS_DUTY) * 8u);
        v.timing_reliable = true; /* nothing to time; the duty figure carries it */
        return v;
    }

    /* Seen something, but not enough complete cycles to judge its rhythm. */
    if(c->bursts < SPECTER_MIN_BURSTS) {
        v.klass = EmitterClassUnknown;
        v.confidence = (uint8_t)(c->bursts * 15u);
        return v;
    }

    /* Regularity decides it. */
    uint32_t ratio = 100u;
    if(c->period_ms) ratio = ((uint32_t)c->jitter_ms * 100u) / c->period_ms;
    if(ratio > 100u) ratio = 100u;

    /* The more complete cycles we watched, the more the regularity figure is
     * worth. Saturates quickly - five clean cycles is already convincing. */
    uint32_t evidence = (uint32_t)c->bursts * 8u;
    if(evidence > 40u) evidence = 40u;

    uint32_t conf;
    if(ratio <= POLLING_JITTER_PCT) {
        v.klass = EmitterClassPolling;
        conf = 55u + evidence + (POLLING_JITTER_PCT - ratio);
    } else {
        v.klass = EmitterClassIntermittent;
        /* Wildly irregular is itself a confident finding; mildly irregular sits
         * on the fence between the two classes and should say so. */
        conf = 35u + evidence + (ratio > 60u ? 15u : 0u);
    }

    /* Timing we could not resolve must not produce a confident cadence call. */
    if(!v.timing_reliable) conf = (conf * 3u) / 4u;

    v.confidence = (uint8_t)(conf > 100u ? 100u : conf);
    return v;
}

const char* emitter_class_name(EmitterClass k) {
    switch(k) {
    case EmitterClassNoField:
        return "NO FIELD";
    case EmitterClassContinuous:
        return "CONTINUOUS";
    case EmitterClassPolling:
        return "POLLING";
    case EmitterClassIntermittent:
        return "INTERMITTENT";
    case EmitterClassUnknown:
    default:
        return "SAMPLING";
    }
}

const char* emitter_class_blurb(EmitterClass k) {
    switch(k) {
    case EmitterClassNoField:
        return "Air is clear";
    case EmitterClassContinuous:
        return "Carrier held up";
    case EmitterClassPolling:
        return "Fixed poll cycle";
    case EmitterClassIntermittent:
        return "Irregular bursts";
    case EmitterClassUnknown:
    default:
        return "Need more cycles";
    }
}
