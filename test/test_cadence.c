/* Host tests for carrier cadence tracking.
 *
 * Both of the bugs these guard against shipped, and both were invisible because
 * this algorithm used to live inside the sampling worker where no test could
 * reach it. They are reproduced here first, as the reports described them. */

#include "../helpers/cadence.h"
#include "../helpers/emitter_classify.h"

#include <stdio.h>

static int failures = 0, checks = 0;
static void check(int cond, const char* what) {
    checks++;
    if(!cond) { failures++; printf("  FAIL: %s\n", what); }
}

/* Drive the tracker with a square wave, one sample every 2 ms as the real
 * sampler does. Returns the tick it left off at. */
static uint32_t
    poll(CadenceTracker* t, uint32_t now, uint32_t on_ms, uint32_t off_ms, unsigned cycles) {
    for(unsigned c = 0; c < cycles; c++) {
        for(uint32_t i = 0; i < on_ms; i += 2) cadence_tracker_sample(t, true, now + i);
        now += on_ms;
        for(uint32_t i = 0; i < off_ms; i += 2) cadence_tracker_sample(t, false, now + i);
        now += off_ms;
    }
    return now;
}

static EmitterVerdict verdict_of(const CadenceTracker* t, uint8_t duty) {
    CadenceStats s;
    cadence_tracker_summarise(t, &s, duty);
    return emitter_classify(&s);
}

int main(void) {
    printf("cadence\n");

    /* --- a textbook polling reader reads as one --------------------------- */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        poll(&t, 0, 50, 150, 16);
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.period_ms >= 195 && s.period_ms <= 205, "period is ~200ms");
        check(s.burst_ms >= 45 && s.burst_ms <= 55, "burst is ~50ms");
        check(s.jitter_ms <= 2, "a crystal-timed poll has ~no jitter");
        check(emitter_classify(&s).klass == EmitterClassPolling, "classified POLLING");
    }

    /* --- REGRESSION 1: walking between two readers ------------------------
     * Twenty seconds of silence between two identical readers used to be
     * pushed in as a single "cycle" with a 20000 ms gap, dragging the mean
     * period and jitter far enough to read INTERMITTENT for 16 more cycles. */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        uint32_t now = poll(&t, 0, 50, 150, 16);
        check(verdict_of(&t, 25).klass == EmitterClassPolling, "POLLING at reader A");

        now += 20000; // the walk
        now = poll(&t, now, 50, 150, 1); // first edge at reader B

        EmitterVerdict v = verdict_of(&t, 25);
        check(v.klass == EmitterClassPolling, "still POLLING after a 20s walk");
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.period_ms <= 260, "the walk is not averaged in as a period");
        check(s.jitter_ms <= 60, "the walk does not blow up the jitter");
    }

    /* --- REGRESSION 1b: a long-held carrier that finally drops ------------- */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        uint32_t now = poll(&t, 0, 50, 150, 16);
        /* five minutes of continuous carrier, then it stops */
        for(uint32_t i = 0; i < 300000; i += 1000) cadence_tracker_sample(&t, true, now + i);
        now += 300000;
        now = poll(&t, now, 50, 150, 1);
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.burst_ms <= 260, "a 5-minute carrier is not recorded as a burst");
        check(verdict_of(&t, 25).klass == EmitterClassPolling, "and it still reads POLLING");
    }

    /* --- REGRESSION 2: the emitter leaves --------------------------------
     * bursts only ever counted up, so the classifier's silence branch became
     * unreachable and Fingerprint kept claiming POLLING at full confidence in
     * an empty room. */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        poll(&t, 0, 50, 150, 16);
        check(verdict_of(&t, 25).confidence >= 90, "confident while it is there");

        cadence_tracker_drop(&t); // presence released

        EmitterVerdict gone = verdict_of(&t, 0);
        check(gone.klass == EmitterClassNoField, "NO FIELD once it is gone and quiet");
        check(gone.confidence == 100, "and certain about the silence");

        /* while the smoothed duty is still decaying it must not claim a class */
        EmitterVerdict decaying = verdict_of(&t, 12);
        check(decaying.klass == EmitterClassUnknown, "SAMPLING while the duty decays");
        check(decaying.confidence < 50, "and it does not claim confidence");
    }

    /* --- a continuous-wave emitter still classifies ----------------------- */
    {
        /* Its ON run exceeds the phase ceiling, so no cycle is ever recorded -
         * CONTINUOUS must be decided on duty alone, before the burst test. */
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        for(uint32_t i = 0; i < 10000; i += 2) cadence_tracker_sample(&t, true, i);
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 99);
        check(s.bursts == 0, "a held carrier records no cycles");
        check(emitter_classify(&s).klass == EmitterClassContinuous, "still reads CONTINUOUS");
    }

    /* --- reset re-anchors, it does not donate the run in progress ---------- */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        for(uint32_t i = 0; i < 4000; i += 2) cadence_tracker_sample(&t, true, i);
        /* reset while the carrier is UP - the old code kept run_start and
         * handed the whole earlier ON time over as the next burst */
        cadence_tracker_reset(&t, true, 4000, 1000);
        uint32_t now = 4000;
        for(uint32_t i = 0; i < 50; i += 2) cadence_tracker_sample(&t, true, now + i);
        now += 50;
        now = poll(&t, now, 0, 150, 1);
        now = poll(&t, now, 50, 150, 3);
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.burst_ms <= 260, "the pre-reset carrier time is not donated");
    }

    /* --- the ring stays bounded ------------------------------------------- */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 1000);
        poll(&t, 0, 50, 150, 500);
        check(t.count == SPECTER_CADENCE_RING, "ring saturates at its size");
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.period_ms >= 195 && s.period_ms <= 205, "and still averages correctly");
    }

    /* --- tick wraparound --------------------------------------------------- */
    {
        CadenceTracker t;
        uint32_t near_end = 0xFFFFFF00u;
        cadence_tracker_reset(&t, false, near_end, 1000);
        poll(&t, near_end, 50, 150, 8); // runs past 2^32
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.period_ms >= 195 && s.period_ms <= 205, "period survives the wrap");
        check(s.jitter_ms <= 2, "jitter survives the wrap");
    }

    /* --- ticks are not milliseconds by definition -------------------------
     * Everything downstream is labelled ms. On a 2 kHz scheduler the same
     * physical 200 ms cycle arrives as 400 ticks, and without conversion the
     * Fingerprint screen would print "400ms" for a 200 ms reader. */
    {
        CadenceTracker t;
        cadence_tracker_reset(&t, false, 0, 2000); // 2 kHz scheduler
        uint32_t now = 0;
        for(unsigned c = 0; c < 16; c++) {
            for(uint32_t i = 0; i < 100; i += 2) cadence_tracker_sample(&t, true, now + i);
            now += 100; // 100 ticks = 50 ms
            for(uint32_t i = 0; i < 300; i += 2) cadence_tracker_sample(&t, false, now + i);
            now += 300; // 300 ticks = 150 ms
        }
        CadenceStats s;
        cadence_tracker_summarise(&t, &s, 25);
        check(s.period_ms >= 195 && s.period_ms <= 205, "reports real ms, not ticks");
        check(s.burst_ms >= 45 && s.burst_ms <= 55, "burst in real ms too");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
