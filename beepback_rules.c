/*
 * The parts of BEEPBACK that decide what you press and what it is worth.
 * No drawing, no input, no Flipper API beyond the clock, so the whole file
 * compiles and runs on a host machine under test/run_tests.sh.
 */
#include "beepback.h"

/* ------------------------------------------------------------------ */
/* Tables                                                              */
/* ------------------------------------------------------------------ */

const uint16_t bb_time_ms[BB_DIFF_COUNT] =
    {BB_TIME_EASY, BB_TIME_NORMAL, BB_TIME_HARD, BB_TIME_INSANE};

const uint16_t bb_rx_shrink[BB_DIFF_COUNT] = {4, 6, 9, 14};
const uint16_t bb_rx_gap[BB_SPEED_COUNT] = {900, 650, 420};

const uint16_t bb_speed_tone[BB_SPEED_COUNT] = {290, 220, 160};
const uint16_t bb_speed_gap[BB_SPEED_COUNT] = {180, 110, 75};

uint16_t bb_window_ms(BbMode mode, uint8_t diff) {
    if(diff >= BB_DIFF_COUNT) diff = 1;
    uint16_t w = bb_time_ms[diff];
    if(mode == BbModeRules || bb_is_challenge(mode)) w += BB_RULE_BONUS;
    return w;
}

/* ------------------------------------------------------------------ */
/* Rules                                                               */
/* ------------------------------------------------------------------ */

void bb_apply_rule(const BbSeq* seq, BbRule rule, uint8_t a, uint8_t b, BbPresses* out) {
    out->len = 0;
    if(seq->len == 0) return;

    switch(rule) {
    case BbRuleSkip:
        for(uint8_t i = 0; i < seq->len; i++)
            if(seq->step[i] != a) out->press[out->len++] = seq->step[i];
        break;

    case BbRuleDouble:
        for(uint8_t i = 0; i < seq->len && out->len < BB_MAX_PRESS - 1; i++) {
            out->press[out->len++] = seq->step[i];
            if(seq->step[i] == a) out->press[out->len++] = seq->step[i];
        }
        break;

    case BbRuleNoDoubles:
        for(uint8_t i = 0; i < seq->len; i++)
            if(i == 0 || seq->step[i] != seq->step[i - 1]) out->press[out->len++] = seq->step[i];
        break;

    case BbRuleEveryOther:
        for(uint8_t i = 0; i < seq->len; i += 2) out->press[out->len++] = seq->step[i];
        break;

    case BbRuleLastTwice:
        for(uint8_t i = 0; i < seq->len && out->len < BB_MAX_PRESS - 1; i++)
            out->press[out->len++] = seq->step[i];
        out->press[out->len++] = seq->step[seq->len - 1];
        break;

    case BbRuleSwap:
        for(uint8_t i = 0; i < seq->len; i++)
            out->press[out->len++] = (seq->step[i] == a) ? b : seq->step[i];
        break;

    case BbRuleBackwards:
        for(int16_t i = seq->len - 1; i >= 0; i--) out->press[out->len++] = seq->step[i];
        break;

    default:
        for(uint8_t i = 0; i < seq->len; i++) out->press[out->len++] = seq->step[i];
        break;
    }
}

bool bb_rule_fits(const BbSeq* seq, BbRule rule, uint8_t a, uint8_t b) {
    if(seq->len == 0) return false;
    BbSeq prefix;
    BbPresses p;

    /* 1. No prefix may leave the player nothing to press. The round is
       built one step at a time, so every length along the way is a stage
       somebody has to play, and this is what makes starting at one safe. */
    for(uint8_t n = 1; n <= seq->len; n++) {
        prefix.len = n;
        memcpy(prefix.step, seq->step, n);
        bb_apply_rule(&prefix, rule, a, b, &p);
        if(p.len == 0) return false;
    }

    bb_apply_rule(seq, rule, a, b, &p);

    /* 2. A press list of one button repeated is mashing, not a round. */
    if(seq->len >= 3) {
        uint8_t first = p.press[0];
        bool two = false;
        for(uint8_t i = 1; i < p.len; i++)
            if(p.press[i] != first) {
                two = true;
                break;
            }
        if(!two) return false;
    }

    /* 3. The rule has to actually change the full sequence. A round that
       announces SKIP DOWN and never plays a DOWN is a dead round, and
       from the player's side it looks exactly like the rule being
       ignored - which is a bug this game has had once already. */
    if(p.len != seq->len) return true;
    for(uint8_t i = 0; i < seq->len; i++)
        if(p.press[i] != seq->step[i]) return true;
    return false;
}

/* ------------------------------------------------------------------ */
/* Deterministic generator                                             */
/*                                                                     */
/* mulberry32, the same one the browser build uses, so a given seed     */
/* produces the same run on a phone and on the device.                  */
/* ------------------------------------------------------------------ */

void bb_rng_seed(BbRng* r, uint32_t seed) {
    r->state = seed;
}

uint32_t bb_rng_next(BbRng* r) {
    r->state += 0x6D2B79F5u;
    uint32_t t = r->state;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return t ^ (t >> 14);
}

uint8_t bb_rng_below(BbRng* r, uint8_t n) {
    if(n == 0) return 0;
    /* Must match Math.floor(rng() * n) in the browser build, which scales the
       32-bit draw to a fraction first. A plain modulo gives a different
       sequence, and then the daily is not the same run on both. */
    return (uint8_t)(((uint64_t)bb_rng_next(r) * n) >> 32);
}

/* A test that pins the daily has to name the date it is pinning, or it
   is asserting against the clock: it passes all day and fails at
   midnight with no code change, which is the worst kind of failure
   because it arrives detached from its cause. Zero means "use the real
   date", which is what the firmware always runs with. */
uint32_t bb_seed_override = 0;

uint32_t bb_today_seed(void) {
    if(bb_seed_override) return bb_seed_override;
#ifdef BB_HOST_TEST
    return 20260910u;
#else
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return (uint32_t)dt.year * 10000u + (uint32_t)dt.month * 100u + dt.day;
#endif
}
