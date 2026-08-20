/* Host tests for presence debouncing.
 *
 * This is the bug that froze the app in the field: a polling reader made the
 * raw per-window verdict alternate true/false several times a second, which
 * counted dozens of phantom contacts and fired an alert sequence for each one
 * until the notification queue backed up and took the GUI thread with it. */

#include "../helpers/present_hold.h"

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

int main(void) {
    printf("present_hold\n");
    /* A representative hold; the adaptive sizing is exercised separately below. */
    const uint32_t HOLD = SPECTER_PRESENT_HOLD_MAX_MS;

    /* --- nothing there ---------------------------------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        check(!present_hold_update(&h, false, 0, HOLD), "silence is not presence");
        check(!present_hold_update(&h, false, 100000, HOLD), "still nothing much later");
    }

    /* --- the reported bug -------------------------------------------------
     * A reader polling every 200 ms, sampled in ~96 ms windows: roughly every
     * other window sees nothing. Presence must stay solid through the gaps. */
    {
        PresentHold h;
        present_hold_reset(&h);
        int drops = 0;
        for(uint32_t t = 0; t <= 10000; t += 96) {
            bool hit = (t % 200u) < 96u; // caught the burst this window or not
            if(!present_hold_update(&h, hit, t, HOLD)) drops++;
        }
        check(drops == 0, "a polling reader never reads as absent");
    }

    /* --- one contact, not many -------------------------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        bool prev = false;
        int rising_edges = 0;
        for(uint32_t t = 0; t <= 10000; t += 96) {
            bool hit = (t % 200u) < 96u;
            bool now = present_hold_update(&h, hit, t, HOLD);
            if(now && !prev) rising_edges++;
            prev = now;
        }
        check(rising_edges == 1, "ten seconds on one reader is one contact");
    }

    /* --- it does let go --------------------------------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        check(present_hold_update(&h, true, 1000, HOLD), "a hit asserts presence");
        check(present_hold_update(&h, false, 1000 + HOLD - 1, HOLD), "holds within the window");
        check(!present_hold_update(&h, false, 1000 + HOLD, HOLD), "releases at the window");
        check(!present_hold_update(&h, false, 1000 + HOLD + 5000, HOLD), "stays released");
    }

    /* --- walking away then back is two contacts --------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        bool prev = false;
        int edges = 0;
        uint32_t t = 0;
        for(; t < 3000; t += 100) { // on a reader
            bool now = present_hold_update(&h, (t % 200u) < 100u, t, HOLD);
            if(now && !prev) edges++;
            prev = now;
        }
        for(; t < 3000 + 2 * HOLD; t += 100) { // walked off
            bool now = present_hold_update(&h, false, t, HOLD);
            if(now && !prev) edges++;
            prev = now;
        }
        for(; t < 3000 + 2 * HOLD + 3000; t += 100) { // came back
            bool now = present_hold_update(&h, (t % 200u) < 100u, t, HOLD);
            if(now && !prev) edges++;
            prev = now;
        }
        check(edges == 2, "leaving and returning counts twice");
    }

    /* --- reset forgets ---------------------------------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        present_hold_update(&h, true, 5000, HOLD);
        present_hold_reset(&h);
        check(!present_hold_update(&h, false, 5001, HOLD), "reset clears the latch");
    }

    /* --- survives the tick counter wrapping ------------------------------- */
    {
        PresentHold h;
        present_hold_reset(&h);
        uint32_t near_end = 0xFFFFFF00u;
        check(present_hold_update(&h, true, near_end, HOLD), "hit just before wrap");
        uint32_t after_wrap = near_end + 200u; // wrapped past 2^32
        check(present_hold_update(&h, false, after_wrap, HOLD), "still present across the wrap");
        check(
            !present_hold_update(&h, false, near_end + HOLD, HOLD),
            "and still releases correctly across the wrap");
    }

    /* --- the hold adapts to the reader's own rhythm -----------------------
     * A flat 1500 ms hold meant the alarm stayed up for ~2 s after the reader
     * was taken away, which reads as a frozen screen. */
    {
        check(
            present_hold_ms_for(0) == SPECTER_PRESENT_HOLD_MIN_MS,
            "an unmeasured period falls back to the floor");
        check(
            present_hold_ms_for(200) == SPECTER_PRESENT_HOLD_MIN_MS,
            "a fast poller releases at the floor, not 1.5s");
        check(present_hold_ms_for(400) == 1000, "2.5 cycles of a 400ms poll");
        check(
            present_hold_ms_for(5000) == SPECTER_PRESENT_HOLD_MAX_MS,
            "an absurdly slow period is capped");
        check(
            SPECTER_PRESENT_HOLD_MIN_MS < 1000u,
            "the floor is under a second so release feels immediate");

        /* it must still be long enough to bridge that reader's own gaps */
        for(uint32_t period = 100; period <= 500; period += 50) {
            check(
                present_hold_ms_for(period) > period * 2u,
                "the hold always outlasts two poll cycles");
        }
    }

    /* A 300ms poller must stay solid, then release promptly once it stops. */
    {
        PresentHold h;
        present_hold_reset(&h);
        uint32_t hold = present_hold_ms_for(300);
        int drops = 0;
        uint32_t t = 0;
        for(; t < 6000; t += 96) {
            if(!present_hold_update(&h, (t % 300u) < 96u, t, hold)) drops++;
        }
        check(drops == 0, "300ms poller never flickers");

        uint32_t gone_at = t;
        while(present_hold_update(&h, false, t, hold) && t < gone_at + 5000) t += 96;
        check(t - gone_at <= 1000, "and lets go within a second of removal");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
