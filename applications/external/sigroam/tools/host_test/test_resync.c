#include "sr_test.h"

#include "sr_resync.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SrResyncAct
    step(SrResyncCtx* c, bool vbus, SrSessionState sess, uint32_t rev, uint32_t rx, uint32_t now) {
    SrResyncIn in;

    memset(&in, 0, sizeof(in));
    in.vbus_present = vbus;
    in.session = sess;
    in.session_rev = rev;
    in.rx_bytes = rx;
    in.now_ms = now;
    return sr_resync_eval(c, &in);
}

static void arm_wait_peer(SrResyncCtx* c, uint32_t now, uint32_t rev, uint32_t rx) {
    SrResyncAct a;

    sr_resync_init(c);
    a = step(c, true, SrSessionRunning, rev, rx, now);
    CHECK(a == SrResyncActNone);
    CHECK(c->phase == SrResyncIdle);
    a = step(c, false, SrSessionRunning, rev, rx, now + 1u);
    CHECK(a == SrResyncActNone);
    CHECK(c->phase == SrResyncWaitPeer);
}

/* Drive WaitPeer through one rx growth and the quiet gap. Does not send on growth. */
static SrResyncAct grow_then_quiet(
    SrResyncCtx* c, uint32_t rev, uint32_t rx_new, uint32_t grow_ms) {
    SrResyncAct a;
    uint32_t q = (uint32_t)SR_RESYNC_RX_QUIET_MS;

    a = step(c, false, SrSessionRunning, rev, rx_new, grow_ms);
    CHECK(a == SrResyncActNone);
    CHECK(c->phase == SrResyncWaitPeer);
    a = step(c, false, SrSessionRunning, rev, rx_new, grow_ms + q - 1u);
    CHECK(a == SrResyncActNone);
    CHECK(c->phase == SrResyncWaitPeer);
    return step(c, false, SrSessionRunning, rev, rx_new, grow_ms + q);
}

int test_resync_run(void) {
    SrResyncCtx c;
    SrResyncAct a;
    unsigned i;
    unsigned n_unarmed = 0;
    unsigned n_edge = 0;
    unsigned n_stall = 0;
    unsigned n_rxgo = 0;
    unsigned n_stopok = 0;
    unsigned n_startok = 0;
    unsigned n_stayrun = 0;
    unsigned n_stopto = 0;
    unsigned n_startto = 0;
    unsigned n_tries = 0;
    unsigned n_giveup = 0;
    unsigned n_manual = 0;
    unsigned n_wrap = 0;
    unsigned n_quiet = 0;
    uint32_t now;
    uint32_t q = (uint32_t)SR_RESYNC_RX_QUIET_MS;
    uint32_t t0;

    sr_test_failures = 0;

    CHECK(sr_resync_eval(NULL, NULL) == SrResyncActNone);
    CHECK(sr_resync_hint_stage(NULL) == (uint8_t)SR_RESYNC_HINT_NONE);

    /* 1. session != Running + VBUS falling edge → do not trigger */
    sr_resync_init(&c);
    (void)step(&c, true, SrSessionStopped, 4u, 100u, 10u);
    a = step(&c, false, SrSessionStopped, 4u, 100u, 11u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncIdle);
    sr_resync_init(&c);
    (void)step(&c, true, SrSessionIdle, 0u, 0u, 10u);
    a = step(&c, false, SrSessionIdle, 0u, 0u, 11u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncIdle);
    n_unarmed++;

    /* 2. armed + VBUS true→false → WaitPeer */
    arm_wait_peer(&c, 100u, 7u, 1000u);
    CHECK(c.rx_at_trig == 1000u);
    CHECK(c.rev_at_trig == 7u);
    CHECK(sr_resync_hint_stage(&c) == (uint8_t)SR_RESYNC_HINT_BUSY);
    n_edge++;

    /* 3. mis-fire surface: armed, no edge, rx frozen for 82+ ticks */
    sr_resync_init(&c);
    now = 1000u;
    a = step(&c, true, SrSessionRunning, 3u, 50u, now);
    CHECK(c.phase == SrResyncIdle);
    for(i = 0; i < 90u; i++) {
        now += 1000u;
        a = step(&c, true, SrSessionRunning, 3u, 50u, now);
        CHECK(a == SrResyncActNone);
        CHECK(c.phase == SrResyncIdle);
    }
    CHECK(sr_resync_hint_stage(&c) == (uint8_t)SR_RESYNC_HINT_NONE);
    n_stall++;

    /* 4. WaitPeer + rx growth: do NOT send until quiet Q ms (A, 2026-09-03).
     * Growth without quiet staying in WaitPeer is the discriminant: reverting
     * to "any +1 byte → stopscan" makes this CHECK fail. */
    arm_wait_peer(&c, 200u, 7u, 1000u);
    a = step(&c, false, SrSessionRunning, 7u, 1001u, 400u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1001u, 400u + q - 1u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1001u, 400u + q);
    CHECK(a == SrResyncActSendStop);
    CHECK(c.phase == SrResyncNeedStop);
    n_rxgo++;

    /* 4b. further rx growth resets the quiet timer */
    arm_wait_peer(&c, 200u, 7u, 1000u);
    a = step(&c, false, SrSessionRunning, 7u, 1001u, 400u);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1002u, 400u + q - 1u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1002u, 400u + q);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1002u, 400u + q - 1u + q);
    CHECK(a == SrResyncActSendStop);
    CHECK(c.phase == SrResyncNeedStop);
    n_quiet++;

    /* 4c. Q ms with no rx growth at all must not send (not a sleep from VBUS). */
    arm_wait_peer(&c, 200u, 7u, 1000u);
    a = step(&c, false, SrSessionRunning, 7u, 1000u, 200u + 1u + q);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 7u, 1000u, 200u + 1u + q + 5000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);

    /* 5. step-1 confirmed (rev rose, Stopped) → SendStart */
    arm_wait_peer(&c, 200u, 7u, 1000u);
    a = grow_then_quiet(&c, 7u, 1001u, 400u);
    CHECK(a == SrResyncActSendStop);
    t0 = 400u + q;
    sr_resync_note_sent(&c, t0, false, 7u);
    CHECK(c.phase == SrResyncAwaitStop);
    CHECK(c.tries == 1u);
    a = step(&c, false, SrSessionStopped, 8u, 1100u, t0 + 100u);
    CHECK(a == SrResyncActSendStart);
    CHECK(c.phase == SrResyncNeedStart);
    n_stopok++;

    /* 6. step-2 confirmed (rev rose again, Running) → Idle */
    sr_resync_note_sent(&c, t0 + 100u, true, 8u);
    CHECK(c.phase == SrResyncAwaitStart);
    CHECK(c.rev_at_step2 == 8u);
    a = step(&c, false, SrSessionRunning, 9u, 1200u, t0 + 200u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncIdle);
    CHECK(sr_resync_hint_stage(&c) == (uint8_t)SR_RESYNC_HINT_NONE);
    n_startok++;

    /* 7. original hole: trigger while already Running must enter WaitPeer, not Idle */
    arm_wait_peer(&c, 300u, 11u, 2000u);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 11u, 2000u, 350u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncWaitPeer);
    n_stayrun++;

    /* 8. step-1 timeout 2000 → fail the sequence; retry from stopscan after 5000 */
    arm_wait_peer(&c, 0u, 1u, 10u);
    a = grow_then_quiet(&c, 1u, 11u, 10u);
    CHECK(a == SrResyncActSendStop);
    t0 = 10u + q;
    sr_resync_note_sent(&c, t0, false, 1u);
    a = step(&c, false, SrSessionRunning, 1u, 11u, t0 + 1999u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncAwaitStop);
    a = step(&c, false, SrSessionRunning, 1u, 11u, t0 + 2000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncNeedStop);
    a = step(&c, false, SrSessionRunning, 1u, 11u, t0 + 4999u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncNeedStop);
    a = step(&c, false, SrSessionRunning, 1u, 11u, t0 + 5000u);
    CHECK(a == SrResyncActSendStop);
    n_stopto++;

    /* 9. C: after stopscan is confirmed, AwaitStart must NOT go back to
     * stopscan at 2000 ms. A late Running still completes. Reverting C
     * (timeout → NeedStop) makes the SendStop check fail. */
    arm_wait_peer(&c, 0u, 2u, 20u);
    a = grow_then_quiet(&c, 2u, 21u, 5u);
    CHECK(a == SrResyncActSendStop);
    t0 = 5u + q;
    sr_resync_note_sent(&c, t0, false, 2u);
    a = step(&c, false, SrSessionStopped, 3u, 22u, t0 + 15u);
    CHECK(a == SrResyncActSendStart);
    sr_resync_note_sent(&c, t0 + 15u, true, 3u);
    a = step(&c, false, SrSessionStopped, 3u, 22u, t0 + 15u + 2000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncAwaitStart);
    a = step(&c, false, SrSessionStopped, 3u, 22u, t0 + 15u + 5000u);
    CHECK(a == SrResyncActNone);
    CHECK(a != SrResyncActSendStop);
    CHECK(c.phase == SrResyncAwaitStart);
    a = step(&c, false, SrSessionRunning, 4u, 22u, t0 + 15u + 12000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncIdle);
    n_startto++;

    /* 9b. AwaitStart with no Running → Lost only at giveup, never SendStop. */
    arm_wait_peer(&c, 0u, 2u, 20u);
    a = grow_then_quiet(&c, 2u, 21u, 5u);
    CHECK(a == SrResyncActSendStop);
    t0 = 5u + q;
    sr_resync_note_sent(&c, t0, false, 2u);
    a = step(&c, false, SrSessionStopped, 3u, 22u, t0 + 15u);
    CHECK(a == SrResyncActSendStart);
    sr_resync_note_sent(&c, t0 + 15u, true, 3u);
    a = step(&c, false, SrSessionStopped, 3u, 22u, c.trigger_ms + 149999u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncAwaitStart);
    CHECK(a != SrResyncActSendStop);
    a = step(&c, false, SrSessionStopped, 3u, 22u, c.trigger_ms + 150000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncLost);

    /* 10. MAX_TRIES failed sequences → Lost, no further send.
     * ⛔ The 24u / 5000u / 2000u below are LITERALS on purpose: they pin the product
     * constants so that editing SR_RESYNC_MAX_TRIES or _RETRY_GAP_MS turns this test red
     * instead of silently tracking the new value. Do not "clean this up" into enum names.
     * Wall clock check: last send at t0 + 23*5000 = 116001, its timeout at 118001, both
     * strictly inside giveup (trigger_ms 1 + 150000), so this case tests tries, not giveup. */
    arm_wait_peer(&c, 0u, 4u, 30u);
    a = grow_then_quiet(&c, 4u, 31u, 1u);
    CHECK(a == SrResyncActSendStop);
    t0 = 1u + q;
    sr_resync_note_sent(&c, t0, false, 4u);
    {
        unsigned k;
        for(k = 1u; k < 24u; k++) {
            a = step(&c, false, SrSessionRunning, 4u, 31u, t0 + (k - 1u) * 5000u + 2000u);
            CHECK(a == SrResyncActNone);
            CHECK(c.phase == SrResyncNeedStop);
            a = step(&c, false, SrSessionRunning, 4u, 31u, t0 + k * 5000u);
            CHECK(a == SrResyncActSendStop);
            sr_resync_note_sent(&c, t0 + k * 5000u, false, 4u);
        }
    }
    CHECK(c.tries == 24u);
    a = step(&c, false, SrSessionRunning, 4u, 31u, t0 + 23u * 5000u + 2000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncLost);
    CHECK(sr_resync_hint_stage(&c) == (uint8_t)SR_RESYNC_HINT_LOST);
    a = step(&c, false, SrSessionRunning, 4u, 31u, t0 + 23u * 5000u + 7000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncLost);
    n_tries++;

    /* 11. 150000 ms from trigger → Lost (WaitPeer, never got rx) */
    arm_wait_peer(&c, 50u, 5u, 40u);
    a = step(&c, false, SrSessionRunning, 5u, 40u, c.trigger_ms + 149999u);
    CHECK(c.phase == SrResyncWaitPeer);
    a = step(&c, false, SrSessionRunning, 5u, 40u, c.trigger_ms + 150000u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncLost);
    n_giveup++;

    /* 12. Lost + operator stop/start (rev rose, Running) → Idle */
    arm_wait_peer(&c, 0u, 6u, 50u);
    a = step(&c, false, SrSessionRunning, 6u, 50u, c.trigger_ms + 150000u);
    CHECK(c.phase == SrResyncLost);
    a = step(&c, false, SrSessionRunning, 8u, 80u, c.trigger_ms + 150100u);
    CHECK(a == SrResyncActNone);
    CHECK(c.phase == SrResyncIdle);
    n_manual++;

    /* 13. tick wrap: unsigned subtraction must not stick in AwaitStop.
     * note_sent plants a pre-wrap cmd_ms; eval now is post-wrap and still
     * near trigger_ms=1 so giveup (also unsigned) does not fire. Quiet uses
     * the same elapsed_ge; covering it here would wrap now vs trigger. */
    arm_wait_peer(&c, 0u, 9u, 60u);
    a = grow_then_quiet(&c, 9u, 61u, 1u);
    CHECK(a == SrResyncActSendStop);
    sr_resync_note_sent(&c, 0xFFFFFFF0u, false, 9u);
    a = step(&c, false, SrSessionRunning, 9u, 61u, 0xFFFFFFF0u + 32u);
    CHECK(c.phase == SrResyncAwaitStop);
    a = step(&c, false, SrSessionRunning, 9u, 61u, 0xFFFFFFF0u + 2000u);
    CHECK(c.phase == SrResyncNeedStop);
    n_wrap++;

    fprintf(
        stderr,
        "resync cover: unarmed=%u edge=%u stall=%u rxgo=%u stopok=%u startok=%u "
        "stayrun=%u stopto=%u startto=%u tries=%u giveup=%u manual=%u wrap=%u "
        "quiet=%u\n",
        n_unarmed,
        n_edge,
        n_stall,
        n_rxgo,
        n_stopok,
        n_startok,
        n_stayrun,
        n_stopto,
        n_startto,
        n_tries,
        n_giveup,
        n_manual,
        n_wrap,
        n_quiet);

    CHECK(n_unarmed == 1u);
    CHECK(n_edge == 1u);
    CHECK(n_stall == 1u);
    CHECK(n_rxgo == 1u);
    CHECK(n_stopok == 1u);
    CHECK(n_startok == 1u);
    CHECK(n_stayrun == 1u);
    CHECK(n_stopto == 1u);
    CHECK(n_startto == 1u);
    CHECK(n_tries == 1u);
    CHECK(n_giveup == 1u);
    CHECK(n_manual == 1u);
    CHECK(n_wrap == 1u);
    CHECK(n_quiet == 1u);

    return sr_test_failures;
}
