/* Host tests for the pure emitter classifier.
 *
 *   make -C test
 *
 * The classifier is the one piece of Specter that turns raw timing into a claim
 * about what is emitting, so it is the piece worth pinning down off-device. No
 * furi, no hardware - just the decision table. */

#include "../helpers/emitter_classify.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void check_class(const CadenceStats* c, EmitterClass want, const char* what) {
    EmitterVerdict v = emitter_classify(c);
    checks++;
    if(v.klass != want) {
        failures++;
        printf(
            "  FAIL: %s -> got %s, want %s\n",
            what,
            emitter_class_name(v.klass),
            emitter_class_name(want));
    }
}

int main(void) {
    printf("emitter_classify\n");

    /* --- silence ------------------------------------------------------- */
    {
        CadenceStats c = {0, 0, 0, 0, 0, 0};
        check_class(&c, EmitterClassNoField, "empty stats");
        EmitterVerdict v = emitter_classify(&c);
        check(v.confidence == 100, "no-field is stated with full confidence");
        check(v.timing_reliable, "no-field timing counts as reliable");
    }

    /* A null pointer must not walk off a cliff. */
    {
        EmitterVerdict v = emitter_classify(NULL);
        check(v.klass == EmitterClassNoField, "NULL is safe");
        check(v.confidence == 0, "NULL claims nothing");
    }

    /* --- continuous wave ------------------------------------------------ */
    {
        CadenceStats c = {0, 0, 0, 0, 0, 100};
        check_class(&c, EmitterClassContinuous, "100% duty");
        EmitterVerdict v = emitter_classify(&c);
        check(v.confidence == 100, "pegged duty is maximally confident");
    }
    {
        /* Just over the line, with tiny dropouts: still effectively CW. */
        CadenceStats c = {12, 190, 8, 198, 3, 96};
        check_class(&c, EmitterClassContinuous, "96% duty with dropouts");
    }
    {
        /* Just under the line must fall through to cadence analysis. */
        CadenceStats c = {10, 90, 10, 100, 2, 94};
        EmitterVerdict v = emitter_classify(&c);
        check(v.klass != EmitterClassContinuous, "94% duty is not continuous");
    }

    /* --- polling -------------------------------------------------------- */
    {
        /* A textbook access reader: 20 ms burst every 200 ms, rock steady. */
        CadenceStats c = {9, 20, 180, 200, 2, 10};
        check_class(&c, EmitterClassPolling, "steady 200ms poll");
        EmitterVerdict v = emitter_classify(&c);
        check(v.confidence >= 80, "steady poll is confident");
        check(v.timing_reliable, "200ms period is well above resolution");
    }
    {
        /* Jitter exactly on the boundary still counts as fixed cadence. */
        CadenceStats c = {6, 30, 170, 200, 30, 15};
        check_class(&c, EmitterClassPolling, "15% jitter is the boundary");
    }
    {
        /* One percent past it is not. */
        CadenceStats c = {6, 30, 170, 200, 32, 15};
        check_class(&c, EmitterClassIntermittent, "16% jitter is irregular");
    }

    /* --- intermittent --------------------------------------------------- */
    {
        CadenceStats c = {7, 40, 260, 300, 210, 13};
        check_class(&c, EmitterClassIntermittent, "wildly irregular bursts");
        EmitterVerdict v = emitter_classify(&c);
        check(v.confidence >= 70, "clear irregularity is itself confident");
    }

    /* --- not enough evidence yet ---------------------------------------- */
    {
        CadenceStats c = {2, 20, 180, 200, 2, 10};
        check_class(&c, EmitterClassUnknown, "two bursts is not a cadence");
        EmitterVerdict v = emitter_classify(&c);
        check(v.confidence < 50, "a guess must not look confident");
    }
    {
        CadenceStats c = {SPECTER_MIN_BURSTS, 20, 180, 200, 2, 10};
        check_class(&c, EmitterClassPolling, "three bursts is enough");
    }

    /* --- honesty about the sampling floor -------------------------------- */
    {
        /* Period below MIN_PERIOD_SAMPLES * SAMPLE_MS: we are reading our own
         * sampler, not the reader. Flag it and discount the confidence. */
        CadenceStats c = {20, 2, 4, 6, 0, 33};
        EmitterVerdict v = emitter_classify(&c);
        check(!v.timing_reliable, "sub-resolution period is flagged unreliable");

        CadenceStats slow = {20, 20, 180, 200, 0, 33};
        EmitterVerdict sv = emitter_classify(&slow);
        check(sv.timing_reliable, "resolvable period is not flagged");
        check(
            v.confidence < sv.confidence,
            "unreliable timing is less confident than the same shape resolved");
    }
    {
        /* Burst narrower than two samples is equally unresolvable, even when
         * the overall period is long. */
        CadenceStats c = {20, 2, 198, 200, 2, 1};
        EmitterVerdict v = emitter_classify(&c);
        check(!v.timing_reliable, "sub-resolution burst is flagged unreliable");
    }

    /* --- display strings are always usable -------------------------------- */
    {
        for(int k = EmitterClassNoField; k <= EmitterClassIntermittent; k++) {
            const char* n = emitter_class_name((EmitterClass)k);
            const char* b = emitter_class_blurb((EmitterClass)k);
            check(n && *n, "class name is non-empty");
            check(b && *b, "class blurb is non-empty");
            /* The fingerprint screen budgets 12 characters for the name. */
            check(strlen(n) <= 12, "class name fits the screen");
        }
        check(
            strcmp(emitter_class_name((EmitterClass)99), "SAMPLING") == 0,
            "unknown enum degrades gracefully");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
