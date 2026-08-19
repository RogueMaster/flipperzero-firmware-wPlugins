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
    const uint32_t HOLD = SPECTER_PRESENT_HOLD_MS;

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

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
