/* Host tests for the duty -> meter scaling.
 *
 *   make -C test
 *
 * This is the layer that decides what the user actually sees on the gauge, and
 * it exists because of a real field report: a Flipper laid directly on a reader
 * never read above ~31%. These tests pin down that a saturating reader now
 * reads like one. */

#include "../helpers/field_scale.h"

#include <stdio.h>

static int failures = 0;
static int checks = 0;

static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

static void check_eq(uint8_t got, uint8_t want, const char* what) {
    checks++;
    if(got != want) {
        failures++;
        printf("  FAIL: %s -> got %u, want %u\n", what, (unsigned)got, (unsigned)want);
    }
}

int main(void) {
    printf("field_scale\n");

    const uint8_t FS = SPECTER_FULL_SCALE_DUTY;

    /* --- the reported bug -------------------------------------------------
     * A payment terminal / access reader polling ~30% of the time, with the
     * Flipper sitting on top of it. Before scaling this displayed "31". */
    {
        uint8_t on_top = field_scale_apply(31, FS);
        check(on_top >= 85, "a saturating reader reads high, not ~31");
        check_eq(field_scale_apply(35, FS), 100, "at full scale the meter pegs");
        check(field_scale_is_saturated(35, FS), "full scale counts as saturated");
        check(field_scale_is_saturated(31, FS) == false, "just below is not saturated");
    }

    /* --- ends and clamping ------------------------------------------------ */
    {
        check_eq(field_scale_apply(0, FS), 0, "silence stays silent");
        check_eq(field_scale_apply(100, FS), 100, "continuous wave pegs");
        check_eq(field_scale_apply(200, FS), 100, "out-of-range input clamps");
        check_eq(field_scale_apply(FS + 1, FS), 100, "above full scale clamps");
    }

    /* --- proportionality in the useful band ------------------------------- */
    {
        /* Half of full scale should read about half the dial. */
        uint8_t half = field_scale_apply(FS / 2, FS);
        check(half >= 45 && half <= 55, "half full-scale reads mid-dial");

        /* Monotonic: closing in must never move the needle backwards. */
        uint8_t prev = 0;
        for(unsigned raw = 0; raw <= 100; raw++) {
            uint8_t v = field_scale_apply((uint8_t)raw, FS);
            check(v >= prev, "scaling is monotonic");
            prev = v;
        }
    }

    /* --- the proximity vocabulary is now reachable -------------------------
     * sweep_view calls a reading STRONG at >=70 and CLOSE at >=45. Against raw
     * duty those were unreachable for a polling reader, so the meter only ever
     * said FAINT/NEAR. They must be reachable now. */
    {
        check(field_scale_apply(25, FS) >= 70, "a solid poll reads STRONG");
        check(field_scale_apply(16, FS) >= 45, "a moderate poll reads CLOSE");
        check(field_scale_apply(3, FS) < 20, "background noise still reads FAINT");
    }

    /* --- raw mode is an exact identity ------------------------------------ */
    {
        for(unsigned raw = 0; raw <= 100; raw++) {
            check_eq(
                field_scale_apply((uint8_t)raw, SPECTER_SCALE_RAW),
                (uint8_t)raw,
                "raw mode passes the duty through untouched");
        }
        check(
            field_scale_is_saturated(100, SPECTER_SCALE_RAW),
            "raw mode saturates only at a full carrier");
        check(
            !field_scale_is_saturated(99, SPECTER_SCALE_RAW),
            "raw mode is not saturated below 100");
    }

    /* --- a bad full scale must not break the meter ------------------------- */
    {
        check_eq(field_scale_apply(50, 0), 50, "full scale of 0 falls back to raw");
        check_eq(field_scale_apply(0, 0), 0, "full scale of 0 is still safe at zero");
        check_eq(field_scale_apply(7, 1), 100, "absurdly low full scale just pegs");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
