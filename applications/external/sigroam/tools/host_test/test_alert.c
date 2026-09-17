#include "sr_test.h"

#include "sr_alert.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Independent oracle. The implementation early-returns (NULL / rev0 / first
 * sighting / same / else) and uses elapsed < GAP as the still-in-window test.
 * This oracle classifies into a 4-way tag first, then applies side effects;
 * the gap uses elapsed >= GAP as the "window open" test (the complement).
 */
typedef enum {
    OaSkip = 0, /* NULL or rev==0: no writes */
    OaArm, /* last_fix was unknown: record, do not emit */
    OaHold, /* same as last_fix: no writes */
    OaEdge /* last_fix <-> now_st differ */
} OaTag;

static uint8_t oa_now_st(bool fix_now) {
    return fix_now ? 2u : 1u;
}

static OaTag oa_tag(const SrAlertCtx* ctx, uint32_t gps_csv_rev, bool fix_now) {
    if(ctx == NULL || gps_csv_rev == 0u) {
        return OaSkip;
    }
    if(ctx->last_fix == 0u) {
        return OaArm;
    }
    if(ctx->last_fix == oa_now_st(fix_now)) {
        return OaHold;
    }
    return OaEdge;
}

static bool oa_window_open(const SrAlertCtx* ctx, uint32_t tick_ms) {
    uint32_t elapsed;

    if(!ctx->has_emitted) {
        return true; /* first emit is never gated */
    }
    elapsed = (uint32_t)(tick_ms - ctx->last_emit_ms);
    /* Complement of the implementation's (elapsed < GAP). */
    return elapsed >= (uint32_t)SR_ALERT_MIN_GAP_MS;
}

static SrAlertKind oracle_eval(SrAlertCtx* ctx, uint32_t gps_csv_rev, bool fix_now, uint32_t tick_ms) {
    OaTag tag;
    uint8_t now_st;

    tag = oa_tag(ctx, gps_csv_rev, fix_now);
    if(tag == OaSkip) {
        return SrAlertNone;
    }
    now_st = oa_now_st(fix_now);
    if(tag == OaArm) {
        ctx->last_fix = now_st;
        return SrAlertNone;
    }
    if(tag == OaHold) {
        return SrAlertNone;
    }
    ctx->last_fix = now_st;
    if(!oa_window_open(ctx, tick_ms)) {
        return SrAlertNone;
    }
    ctx->last_emit_ms = tick_ms;
    ctx->has_emitted = true;
    return (now_st == 1u) ? SrAlertGpsFixLost : SrAlertGpsFixAcquired;
}

static SrAlertKind both(SrAlertCtx* impl, SrAlertCtx* ora, uint32_t rev, bool fix, uint32_t now) {
    SrAlertKind a;
    SrAlertKind b;

    a = sr_alert_eval(impl, rev, fix, now);
    b = oracle_eval(ora, rev, fix, now);
    CHECK(a == b);
    CHECK(impl->last_fix == ora->last_fix);
    CHECK(impl->last_emit_ms == ora->last_emit_ms);
    CHECK(impl->has_emitted == ora->has_emitted);
    return a;
}

int test_alert_run(void) {
    SrAlertCtx impl;
    SrAlertCtx ora;
    SrAlertKind k;
    unsigned n_null = 0;
    unsigned n_rev0 = 0;
    unsigned n_first_fix = 0;
    unsigned n_first_nofix = 0;
    unsigned n_lost = 0;
    unsigned n_acq = 0;
    unsigned n_gap_block = 0;
    unsigned n_gap_pass = 0;
    unsigned n_same = 0;
    unsigned n_wrap = 0;
    unsigned n_firstemit = 0;

    sr_test_failures = 0;

    /* Pin the product constant with a literal so editing SR_ALERT_MIN_GAP_MS
     * turns this file red instead of silently tracking the new value. */
    CHECK(SR_ALERT_MIN_GAP_MS == 3000);

    /* a: ctx == NULL */
    k = sr_alert_eval(NULL, 1u, true, 10u);
    CHECK(k == SrAlertNone);
    n_null++;

    /* b: gps_csv_rev == 0 leaves last_fix untouched (still unknown, and also
     * when a prior state exists). */
    sr_alert_reset(&impl);
    CHECK(impl.last_fix == 0u);
    CHECK(impl.last_emit_ms == 0u);
    CHECK(impl.has_emitted == false);
    ora = impl;
    k = both(&impl, &ora, 0u, true, 50u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 0u);
    impl.last_fix = 2u;
    impl.has_emitted = true;
    impl.last_emit_ms = 9u;
    ora = impl;
    k = both(&impl, &ora, 0u, false, 99u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 2u);
    CHECK(impl.last_emit_ms == 9u);
    CHECK(impl.has_emitted == true);
    n_rev0++;

    /* c: first sighting with fix: record, do not emit; has_emitted / last_emit_ms
     * stay put. */
    sr_alert_reset(&impl);
    impl.has_emitted = true;
    impl.last_emit_ms = 123u;
    ora = impl;
    k = both(&impl, &ora, 1u, true, 5u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 2u);
    CHECK(impl.has_emitted == true);
    CHECK(impl.last_emit_ms == 123u);
    n_first_fix++;

    /* c: first sighting with no fix. */
    sr_alert_reset(&impl);
    ora = impl;
    k = both(&impl, &ora, 4u, false, 8u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 1u);
    CHECK(impl.has_emitted == false);
    CHECK(impl.last_emit_ms == 0u);
    n_first_nofix++;

    /* h / firstemit: has_emitted == false, so a tiny elapsed still emits. */
    sr_alert_reset(&impl);
    ora = impl;
    (void)both(&impl, &ora, 1u, true, 0u); /* arm as has-fix */
    k = both(&impl, &ora, 2u, false, 1u);
    CHECK(k == SrAlertGpsFixLost);
    CHECK(impl.last_fix == 1u);
    CHECK(impl.has_emitted == true);
    CHECK(impl.last_emit_ms == 1u);
    n_firstemit++;
    n_lost++;

    /* e: no-fix -> has-fix, window open. */
    k = both(&impl, &ora, 3u, true, 1u + 3000u);
    CHECK(k == SrAlertGpsFixAcquired);
    CHECK(impl.last_fix == 2u);
    CHECK(impl.last_emit_ms == 3001u);
    n_acq++;

    /* g: same as last_fix, two polarities. */
    k = both(&impl, &ora, 4u, true, 4000u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 2u);
    CHECK(impl.last_emit_ms == 3001u);
    n_same++;
    k = both(&impl, &ora, 5u, false, 1u + 3000u + 3000u);
    CHECK(k == SrAlertGpsFixLost);
    k = both(&impl, &ora, 6u, false, 9000u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 1u);
    n_same++;

    /* f / gap_block: elapsed exactly 2999 must block; last_fix still updates. */
    sr_alert_reset(&impl);
    impl.last_fix = 2u;
    impl.has_emitted = true;
    impl.last_emit_ms = 1000u;
    ora = impl;
    k = both(&impl, &ora, 7u, false, 1000u + 2999u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 1u);
    CHECK(impl.last_emit_ms == 1000u);
    CHECK(impl.has_emitted == true);
    n_gap_block++;

    /* f / gap_pass: elapsed exactly 3000 must emit (boundary is >=, not >). */
    sr_alert_reset(&impl);
    impl.last_fix = 2u;
    impl.has_emitted = true;
    impl.last_emit_ms = 1000u;
    ora = impl;
    k = both(&impl, &ora, 8u, false, 1000u + 3000u);
    CHECK(k == SrAlertGpsFixLost);
    CHECK(impl.last_fix == 1u);
    CHECK(impl.last_emit_ms == 4000u);
    CHECK(impl.has_emitted == true);
    n_gap_pass++;

    /* wrap: last_emit_ms = 0xFFFFFF00, tick = 0x00000100 -> unsigned diff 512
     * < 3000, must block. A signed or naive last+GAP <= tick comparison is
     * what this case is for. */
    sr_alert_reset(&impl);
    impl.last_fix = 2u;
    impl.has_emitted = true;
    impl.last_emit_ms = 0xFFFFFF00u;
    ora = impl;
    k = both(&impl, &ora, 9u, false, 0x00000100u);
    CHECK(k == SrAlertNone);
    CHECK(impl.last_fix == 1u);
    CHECK(impl.last_emit_ms == 0xFFFFFF00u);
    CHECK(impl.has_emitted == true);
    n_wrap++;

    fprintf(
        stderr,
        "alert cover: null=%u rev0=%u first_fix=%u first_nofix=%u lost=%u acq=%u "
        "gap_block=%u gap_pass=%u same=%u wrap=%u firstemit=%u\n",
        n_null,
        n_rev0,
        n_first_fix,
        n_first_nofix,
        n_lost,
        n_acq,
        n_gap_block,
        n_gap_pass,
        n_same,
        n_wrap,
        n_firstemit);

    CHECK(n_null == 1u);
    CHECK(n_rev0 == 1u);
    CHECK(n_first_fix == 1u);
    CHECK(n_first_nofix == 1u);
    CHECK(n_lost == 1u);
    CHECK(n_acq == 1u);
    CHECK(n_gap_block == 1u);
    CHECK(n_gap_pass == 1u);
    CHECK(n_same == 2u);
    CHECK(n_wrap == 1u);
    CHECK(n_firstemit == 1u);

    return sr_test_failures;
}
