/*
 * Every branch of the grading brain, on the host.
 *
 * rc_analyze is the one place where "what we heard on the air" becomes a
 * letter grade a person acts on, so each row of its decision table gets an
 * explicit case here. The inputs are synthetic capture logs - no radio needed.
 *
 *   make -C test
 */
#include "../helpers/analyzer.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                             \
    do {                                             \
        checks++;                                    \
        if(!(cond)) {                                \
            failures++;                              \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                     \
            printf("\n");                            \
        }                                            \
    } while(0)

/* Build a capture log: `n` presses of `cls`, with `unique` distinct parcels
 * spread across them (the rest repeat the first fingerprint). */
static uint8_t
    make_log(RcCapture* caps, uint8_t n, RcCodeClass cls, uint8_t unique, const char* proto) {
    memset(caps, 0, sizeof(RcCapture) * n);
    for(uint8_t i = 0; i < n; i++) {
        strncpy(caps[i].protocol, proto, sizeof(caps[i].protocol) - 1);
        caps[i].cls = cls;
        caps[i].bits = 64;
        caps[i].rssi = -55;
        caps[i].tick = 1000u * (i + 1);
        /* first `unique` presses each get their own parcel; any beyond that
         * repeat parcel 0, which is what a fob that failed to roll looks like */
        caps[i].fingerprint = (i < unique) ? (0xA000ULL + i) : 0xA000ULL;
    }
    return n;
}

static void test_no_signal(void) {
    printf("no signal at all\n");
    RcVerdict v;
    rc_analyze(NULL, 0, &v);

    CHECK(v.health == RcHealthUnknown, "health %d, want Unknown", (int)v.health);
    CHECK(strcmp(v.letter, "?") == 0, "letter '%s', want '?'", v.letter);
    CHECK(v.presses == 0, "presses %u, want 0", v.presses);
    CHECK(v.unique == 0, "unique %u, want 0", v.unique);
    CHECK(v.meter == 0, "meter %u, want 0", v.meter);
    /* The advice screen is the whole value of a zero-capture run - it must not
     * come back empty. */
    CHECK(strlen(v.detail) > 40, "detail too short (%zu chars)", strlen(v.detail));
}

static void test_rolling_every_press_unique(void) {
    printf("rolling code, every press different -> A\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 3, RcCodeDynamic, 3, "KeeLoq");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthHealthy, "health %d, want Healthy", (int)v.health);
    CHECK(strcmp(v.letter, "A") == 0, "letter '%s', want 'A'", v.letter);
    CHECK(v.cls == RcCodeDynamic, "cls %d, want Dynamic", (int)v.cls);
    CHECK(v.presses == 3 && v.unique == 3, "%u/%u, want 3/3", v.presses, v.unique);
    CHECK(v.meter == 100, "meter %u, want 100", v.meter);
    CHECK(strcmp(v.protocol, "KeeLoq") == 0, "protocol '%s'", v.protocol);
}

static void test_rolling_some_repeats(void) {
    printf("rolling code, some presses repeated -> B\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 4, RcCodeDynamic, 2, "KeeLoq");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthLikely, "health %d, want Likely", (int)v.health);
    CHECK(strcmp(v.letter, "B") == 0, "letter '%s', want 'B'", v.letter);
    CHECK(v.presses == 4 && v.unique == 2, "%u/%u, want 4/2", v.presses, v.unique);
    CHECK(v.meter == 78, "meter %u, want 78", v.meter);
}

static void test_rolling_never_advanced(void) {
    printf("rolling protocol that never advanced -> C\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 3, RcCodeDynamic, 1, "KeeLoq");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthCaution, "health %d, want Caution", (int)v.health);
    CHECK(strcmp(v.letter, "C") == 0, "letter '%s', want 'C'", v.letter);
    CHECK(v.unique == 1, "unique %u, want 1", v.unique);
    CHECK(v.meter == 40, "meter %u, want 40", v.meter);
}

static void test_rolling_single_press(void) {
    printf("rolling protocol, only one press seen -> B (not yet proven)\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 1, RcCodeDynamic, 1, "Nice FloR-S");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthLikely, "health %d, want Likely", (int)v.health);
    CHECK(strcmp(v.letter, "B") == 0, "letter '%s', want 'B'", v.letter);
    CHECK(v.meter == 68, "meter %u, want 68", v.meter);
    /* One press can never earn an A - the roll has not been observed. */
    CHECK(v.health != RcHealthHealthy, "a single press must not grade Healthy");
}

static void test_static_code(void) {
    printf("fixed code -> F\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 3, RcCodeStatic, 1, "Princeton");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthAtRisk, "health %d, want AtRisk", (int)v.health);
    CHECK(strcmp(v.letter, "F") == 0, "letter '%s', want 'F'", v.letter);
    CHECK(v.cls == RcCodeStatic, "cls %d, want Static", (int)v.cls);
    CHECK(v.meter == 8, "meter %u, want 8", v.meter);
}

static void test_static_never_upgrades(void) {
    printf("fixed code stays F even if the parcels differ\n");
    /* A static protocol whose parcels somehow differ (different buttons on the
     * same fob, say) is still replayable - it must not be scored as rolling. */
    RcCapture caps[8];
    uint8_t n = make_log(caps, 3, RcCodeStatic, 3, "CAME");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthAtRisk, "health %d, want AtRisk", (int)v.health);
    CHECK(strcmp(v.letter, "F") == 0, "letter '%s', want 'F'", v.letter);
}

static void test_unclassified(void) {
    printf("decoded but unclassified -> ?\n");
    RcCapture caps[8];
    uint8_t n = make_log(caps, 2, RcCodeUnknown, 2, "RAW");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.health == RcHealthUnknown, "health %d, want Unknown", (int)v.health);
    CHECK(strcmp(v.letter, "?") == 0, "letter '%s', want '?'", v.letter);
    CHECK(v.meter == 0, "meter %u, want 0", v.meter);
}

static void test_full_log(void) {
    printf("a full capture log does not overflow the verdict buffers\n");
    RcCapture caps[RC_MAX_CAPTURES];
    uint8_t n = make_log(
        caps, RC_MAX_CAPTURES, RcCodeDynamic, RC_MAX_CAPTURES, "AnExtremelyLongProtocolName");

    RcVerdict v;
    rc_analyze(caps, n, &v);

    CHECK(v.presses == RC_MAX_CAPTURES, "presses %u", v.presses);
    /* Every string field must still be NUL terminated inside its own array. */
    CHECK(memchr(v.protocol, '\0', sizeof(v.protocol)) != NULL, "protocol not terminated");
    CHECK(memchr(v.headline, '\0', sizeof(v.headline)) != NULL, "headline not terminated");
    CHECK(memchr(v.detail, '\0', sizeof(v.detail)) != NULL, "detail not terminated");
    CHECK(memchr(v.letter, '\0', sizeof(v.letter)) != NULL, "letter not terminated");
}

static void test_labels(void) {
    printf("labels are non-empty for every enum value\n");
    CHECK(strcmp(rc_class_label(RcCodeStatic), "Fixed") == 0, "static label");
    CHECK(strcmp(rc_class_label(RcCodeDynamic), "Rolling") == 0, "dynamic label");
    CHECK(strcmp(rc_class_label(RcCodeUnknown), "Unknown") == 0, "unknown label");

    const RcHealth all[] = {
        RcHealthUnknown, RcHealthAtRisk, RcHealthCaution, RcHealthLikely, RcHealthHealthy};
    for(size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        CHECK(strlen(rc_health_label(all[i])) > 0, "health label %zu empty", i);
    }
}

int main(void) {
    printf("== rc_analyze ==\n");
    test_no_signal();
    test_rolling_every_press_unique();
    test_rolling_some_repeats();
    test_rolling_never_advanced();
    test_rolling_single_press();
    test_static_code();
    test_static_never_upgrades();
    test_unclassified();
    test_full_log();
    test_labels();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
