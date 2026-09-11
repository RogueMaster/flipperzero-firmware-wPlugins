/* Host-side check that the C logic matches the browser build exactly. */
#define BB_HOST_TEST 1
#include <stdio.h>
#include <string.h>
#include "beepback.h"
#include "../beepback_rules.c"

static int fails = 0;
static void check(const char* name, int ok, const char* extra) {
    printf("%s %s%s%s\n", ok ? "ok  " : "FAIL", name, extra && *extra ? "  -> " : "", extra ? extra : "");
    if(!ok) fails++;
}
static const char* seqstr(const uint8_t* v, uint8_t n) {
    static char buf[128]; int p = 0;
    for(uint8_t i = 0; i < n && p < 120; i++) p += sprintf(buf + p, "%u", v[i]);
    buf[p] = 0; return buf;
}

int main(void) {
    char msg[160];

    /* ---- the multipliers the simulation settled on ---- */
    /* The multiplier is two hundredths multiplied together, so it is in
       ten-thousandths: x1.00 is 10000. Keeping the product whole is what
       stops 155 x 145 = 22475 losing its .75, which is a scoring
       difference and not a rounding one. */
    check("normal on normal is exactly x1", bb_multiplier(BbModeClassic, 1, 1) == 10000, "");
    sprintf(msg, "%lu", (unsigned long)bb_multiplier(BbModeClassic, 3, 2));
    check("insane and fast is x8.70", bb_multiplier(BbModeClassic, 3, 2) == 87000, msg);
    check("easy and slow is a discount", bb_multiplier(BbModeClassic, 0, 0) < 10000, "");
    check("reflex has its own time table", bb_multiplier(BbModeReflex, 3, 1) == 23000, "");
    check("a stage of 4 at x1.55 pays 62", bb_apply_mult(40, 15500) == 62, "");
    check("and pays 40 flat at x1.00", bb_apply_mult(40, 10000) == 40, "");

    /* the four corners, against the browser build's own table */
    sprintf(msg, "%lu", (unsigned long)bb_multiplier(BbModeClassic, 0, 0));
    check("easy and slow is 5250, paying 53 on 100",
          bb_multiplier(BbModeClassic, 0, 0) == 5250 && bb_apply_mult(100, 5250) == 53, msg);
    sprintf(msg, "%lu", (unsigned long)bb_multiplier(BbModeClassic, 2, 2));
    check("hard and fast is 22475, paying 225 on 100",
          bb_multiplier(BbModeClassic, 2, 2) == 22475 && bb_apply_mult(100, 22475) == 225, msg);
    check("normal on normal pays exactly what it was given",
          bb_apply_mult(100, 10000) == 100, "");
    check("insane and fast pays 870 on 100", bb_apply_mult(100, 87000) == 870, "");

    /* the pairs that were being truncated before */
    check("easy and fast is x1.02, not x1.01", bb_multiplier(BbModeClassic, 0, 2) == 10150, "");
    check("hard and slow is x1.16", bb_multiplier(BbModeClassic, 2, 0) == 11625, "");
    check("reflex hard and fast is x2.18", bb_multiplier(BbModeReflex, 2, 2) == 21750, "");

    /* the arithmetic stays in 32 bits, so the worst case has to fit */
    {
        uint32_t worst_base = 50u * 255u; /* the biggest round bonus there is */
        uint32_t worst_mult = bb_multiplier(BbModeClassic, 3, 2);
        sprintf(msg, "%lu x %lu", (unsigned long)worst_base, (unsigned long)worst_mult);
        check("the largest award times the largest multiplier still fits",
              worst_base < 0xFFFFFFFFu / worst_mult, msg);
        check("and comes out right", bb_apply_mult(worst_base, worst_mult) == 110925, "");
    }

    /* no two settings may share a label, or the dials lie */
    {
        char seen[BB_DIFF_COUNT * BB_SPEED_COUNT][16];
        int n = 0, clash = 0;
        for(uint8_t d = 0; d < BB_DIFF_COUNT; d++)
            for(uint8_t s = 0; s < BB_SPEED_COUNT; s++) {
                uint32_t m = bb_multiplier(BbModeClassic, d, s);
                uint32_t h = (m + 50u) / 100u;
                sprintf(seen[n], "x%lu.%02lu", (unsigned long)(h / 100u), (unsigned long)(h % 100u));
                for(int i = 0; i < n; i++)
                    if(strcmp(seen[i], seen[n]) == 0) clash++;
                n++;
            }
        sprintf(msg, "%d labels, %d clashes", n, clash);
        check("every time and speed pair reads as a different multiplier", clash == 0, msg);
    }

    /* ---- reflex hit values ---- */
    check("a hit at the start is worth 10", bb_rx_hit_value(1000) == 10, "");
    sprintf(msg, "%lu", (unsigned long)bb_rx_hit_value(250));
    check("a hit at 250ms is worth 160", bb_rx_hit_value(250) == 160, msg);
    check("tighter always pays more", bb_rx_hit_value(300) > bb_rx_hit_value(600), "");

    /* ---- windows ---- */
    check("classic insane is 2000ms", bb_window_ms(BbModeClassic, 3) == 2000, "");
    check("rules gets its extra second", bb_window_ms(BbModeRules, 1) == 5000, "");
    check("challenge gets it too", bb_window_ms(BbModeChallenge, 1) == 5000, "");
    check("daily as well", bb_window_ms(BbModeDaily, 1) == 5000, "");

    /* ---- every rule, against a known sequence ---- */
    BbSeq s = {{BbBtnUp, BbBtnOk, BbBtnOk, BbBtnLeft, BbBtnUp}, 5};   /* 0 4 4 2 0 */
    BbPresses p;

    bb_apply_rule(&s, BbRuleSkip, BbBtnOk, 0, &p);
    check("SKIP drops every one of them", strcmp(seqstr(p.press, p.len), "020") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleDouble, BbBtnUp, 0, &p);
    check("DOUBLE repeats each appearance", strcmp(seqstr(p.press, p.len), "0044200") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleNoDoubles, 0, 0, &p);
    check("NO DOUBLES collapses the pair", strcmp(seqstr(p.press, p.len), "0420") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleEveryOther, 0, 0, &p);
    check("EVERY OTHER takes 1st, 3rd, 5th", strcmp(seqstr(p.press, p.len), "040") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleLastTwice, 0, 0, &p);
    check("LAST TWICE adds one at the end", strcmp(seqstr(p.press, p.len), "044200") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleSwap, BbBtnUp, BbBtnRight, &p);
    check("X IS Y swaps every one", strcmp(seqstr(p.press, p.len), "34423") == 0, seqstr(p.press, p.len));

    bb_apply_rule(&s, BbRuleBackwards, 0, 0, &p);
    check("BACKWARDS reverses it", strcmp(seqstr(p.press, p.len), "02440") == 0, seqstr(p.press, p.len));

    /* ---- the guard against unplayable rounds ---- */
    BbSeq only = {{BbBtnOk, BbBtnOk, BbBtnOk}, 3};
    check("a sequence that empties out is rejected", !bb_rule_fits(&only, BbRuleSkip, BbBtnOk, 0), "");
    check("one button mashed is rejected too", !bb_rule_fits(&only, BbRuleDouble, BbBtnOk, 0), "");
    check("a real sequence is accepted", bb_rule_fits(&s, BbRuleSkip, BbBtnOk, 0), "");

    /* ---- the daily generator ---- */
    BbRng a, b;
    bb_rng_seed(&a, 20260910u);
    bb_rng_seed(&b, 20260910u);
    int same = 1;
    for(int i = 0; i < 50; i++)
        if(bb_rng_next(&a) != bb_rng_next(&b)) same = 0;
    check("the same day gives the same run", same, "50 draws identical");

    bb_rng_seed(&a, 20260910u);
    bb_rng_seed(&b, 20260911u);
    check("a different day does not", bb_rng_next(&a) != bb_rng_next(&b), "");

    /* spread: a generator that favours one button would ruin the game */
    BbRng r;
    bb_rng_seed(&r, 12345u);
    int hist[BbBtnCount] = {0};
    for(int i = 0; i < 5000; i++) hist[bb_rng_below(&r, BbBtnCount)]++;
    int lo = hist[0], hi = hist[0];
    for(int i = 1; i < BbBtnCount; i++) {
        if(hist[i] < lo) lo = hist[i];
        if(hist[i] > hi) hi = hist[i];
    }
    sprintf(msg, "%d..%d of 1000 each", lo, hi);
    check("all five buttons come up evenly", lo > 880 && hi < 1120, msg);

    /* ---- deep runs must not overflow the press buffer ---- */
    BbSeq big;
    big.len = BB_MAX_SEQ;
    bb_rng_seed(&r, 7u);
    for(uint8_t i = 0; i < big.len; i++) big.step[i] = bb_rng_below(&r, BbBtnCount);
    int overflow = 0;
    for(int rule = 0; rule < BB_RULE_COUNT; rule++) {
        bb_apply_rule(&big, (BbRule)rule, BbBtnOk, BbBtnUp, &p);
        if(p.len > BB_MAX_PRESS) overflow = 1;
    }
    sprintf(msg, "%u steps, worst case %u presses", big.len, p.len);
    check("a full sequence never overruns the buffer", !overflow, msg);


    /* ---- the daily must be identical on the device and in the browser ---- */
    {
        /* these twelve came out of the JavaScript build for seed 20260910 */
        const uint8_t js[12] = {1, 2, 3, 1, 0, 2, 1, 1, 4, 3, 3, 4};
        BbRng d;
        bb_rng_seed(&d, 20260910u);
        int match = 1;
        char got[64] = {0};
        int gp = 0;
        for(int i = 0; i < 12; i++) {
            uint8_t v = bb_rng_below(&d, BbBtnCount);
            gp += sprintf(got + gp, "%u", v);
            if(v != js[i]) match = 0;
        }
        check("the same seed gives the browser's sequence", match, got);
    }

    printf(fails ? "\n%d FAILURES\n" : "\nall C logic checks passed\n", fails);
    return fails ? 1 : 0;
}
