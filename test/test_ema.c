/* Host tests for the strength smoother.
 *
 * The bug this guards against is subtle and was live for every release: an
 * integer EMA that discards its remainder cannot reach its own input, so the
 * field meter under-read by about three points of raw duty permanently. */

#include "../helpers/ema.h"

#include <stdio.h>

static int failures = 0, checks = 0;

static void check(int cond, const char* what) {
    checks++;
    if(!cond) {
        failures++;
        printf("  FAIL: %s\n", what);
    }
}

int main(void) {
    printf("ema\n");

    /* --- the bug: it must actually converge -------------------------------- */
    for(unsigned steady = 0; steady <= 100; steady++) {
        Ema e;
        ema_reset(&e);
        uint8_t v = 0;
        for(int i = 0; i < 200; i++) v = ema_update(&e, (uint8_t)steady);
        checks++;
        if(v != steady) {
            failures++;
            printf("  FAIL: steady %u settles at %u\n", steady, v);
        }
    }

    /* --- it still smooths ---------------------------------------------------
     * A step must not be followed instantly, or the meter jitters with every
     * burst instead of riding proximity. */
    {
        Ema e;
        ema_reset(&e);
        uint8_t first = ema_update(&e, 100);
        check(first < 40, "a step is damped, not followed");
        uint8_t v = first;
        for(int i = 0; i < 3; i++) v = ema_update(&e, 100);
        check(v > first, "but it keeps climbing");
    }

    /* --- falling signals must not wrap ------------------------------------- */
    {
        Ema e;
        ema_reset(&e);
        for(int i = 0; i < 60; i++) ema_update(&e, 90);
        uint8_t v = 90;
        for(int i = 0; i < 200; i++) v = ema_update(&e, 0);
        check(v == 0, "falls all the way back to zero");
    }

    /* --- bounds ------------------------------------------------------------ */
    {
        Ema e;
        ema_reset(&e);
        uint8_t v = 0;
        for(int i = 0; i < 200; i++) v = ema_update(&e, 100);
        check(v == 100, "a pegged carrier reads 100, not 97");
        check(ema_update(&e, 100) <= 100, "never exceeds 100");
    }

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
