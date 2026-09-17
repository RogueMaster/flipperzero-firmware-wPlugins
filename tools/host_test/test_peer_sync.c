#include "sr_test.h"

#include "sr_peer_sync.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Oracle: pack the four conditions into a bitfield and look the answer up in a
 * 16-entry table written out by hand. Deliberately not the implementation's
 * if-ladder -- restating it as `all four bits clear` would just be the same
 * expression twice, and a wrong entry in a spelled-out table is visible.
 *   bit0 = !diag_seen
 *   bit1 = cmd_pending
 *   bit2 = diag_state != SCANNING
 *   bit3 = cur == SrSessionRunning
 */
static const SrPeerSyncAct k_oracle[16] = {
    SrPeerSyncAdoptRunning, /* 0000 seen, idle-cmd, SCANNING, not-Running -> the only adopt */
    SrPeerSyncWait,         /* 0001 !seen */
    SrPeerSyncNone,         /* 0010 cmd_pending */
    SrPeerSyncWait,         /* 0011 */
    SrPeerSyncNone,         /* 0100 not SCANNING */
    SrPeerSyncWait,         /* 0101 */
    SrPeerSyncNone,         /* 0110 */
    SrPeerSyncWait,         /* 0111 */
    SrPeerSyncNone,         /* 1000 already Running */
    SrPeerSyncWait,         /* 1001 */
    SrPeerSyncNone,         /* 1010 */
    SrPeerSyncWait,         /* 1011 */
    SrPeerSyncNone,         /* 1100 */
    SrPeerSyncWait,         /* 1101 */
    SrPeerSyncNone,         /* 1110 */
    SrPeerSyncWait,         /* 1111 */
};

static SrBloom g_bloom;
static SrRawLog g_rawlog;

static void model_fresh(SrModel* m) {
    memset(m, 0, sizeof(*m));
    sr_bloom_init(&g_bloom);
    memset(&g_rawlog, 0, sizeof(g_rawlog));
    sr_model_init(m, &g_bloom, &g_rawlog);
}

int test_peer_sync_run(void);

int test_peer_sync_run(void) {
    unsigned seen_i, pend_i, st, cur_i;
    unsigned cov_adopt = 0, cov_none = 0, cov_wait = 0, cov_total = 0;
    unsigned cov_state[8];
    SrModel m;

    memset(cov_state, 0, sizeof(cov_state));

    /* The wire numbering these three asserts pin is also asserted at compile time in
     * sr_peer_sync.h; repeat it here so the test file names the same contract. */
    CHECK((unsigned)SrPeerStIdle == 0u);
    CHECK((unsigned)SrPeerStScanning == 1u);
    CHECK((unsigned)SrPeerStStopping == 2u);
    CHECK((unsigned)SrPeerStDraining == 3u);
    CHECK((unsigned)SrPeerStSealed == 4u);
    CHECK((unsigned)SrPeerStUploading == 5u);

    /* Exhaustive: 2 x 2 x 8 x 3 = 96. st runs to 7 so the two values the firmware
     * cannot currently emit (6, 7) are covered too -- an out-of-range reading must
     * not adopt. */
    for(seen_i = 0; seen_i < 2u; seen_i++) {
        for(pend_i = 0; pend_i < 2u; pend_i++) {
            for(st = 0; st < 8u; st++) {
                for(cur_i = 0; cur_i < 3u; cur_i++) {
                    bool seen = (seen_i != 0u);
                    bool pend = (pend_i != 0u);
                    SrSessionState cur = (SrSessionState)cur_i;
                    unsigned key = 0;
                    SrPeerSyncAct got;

                    if(!seen) key |= 1u;
                    if(pend) key |= 2u;
                    if(st != 1u) key |= 4u;
                    if(cur == SrSessionRunning) key |= 8u;

                    got = sr_peer_sync_eval(seen, (uint8_t)st, cur, pend);
                    CHECK(got == k_oracle[key]);
                    cov_total++;
                    if(got == SrPeerSyncAdoptRunning) {
                        cov_adopt++;
                        cov_state[st]++;
                    } else if(got == SrPeerSyncWait) {
                        cov_wait++;
                    } else {
                        cov_none++;
                    }
                }
            }
        }
    }
    /* The adopt count is not free-floating: exactly one (seen, !pending, st==1) case per
     * non-Running cur value, and Idle/Stopped are the two of them. Asserting the number
     * is what catches a guard that silently stopped guarding. */
    CHECK(cov_adopt == 2u);
    CHECK(cov_wait == 48u);
    CHECK(cov_none == 46u);
    CHECK(cov_state[1] == 2u);
    CHECK(cov_state[0] == 0u);
    CHECK(cov_state[2] == 0u);
    CHECK(cov_state[3] == 0u);
    CHECK(cov_state[4] == 0u);
    CHECK(cov_state[5] == 0u);

    /* Named case A -- the measured failure. Board SCANNING (Diag: st=1, captured
     * 2026-09-09 over the bridge), model fresh from a relaunch. */
    CHECK(sr_peer_sync_eval(true, 1u, SrSessionIdle, false) == SrPeerSyncAdoptRunning);

    /* Named case B -- the cmd_pending guard. Operator pressed OK, stopscan is in flight,
     * and the board still says SCANNING because the stop has not landed. Adopting here
     * would bump session_rev, which is the very signal sr_scan_ctl_eval reads as
     * confirmation of the in-flight command. */
    CHECK(sr_peer_sync_eval(true, 1u, SrSessionIdle, true) == SrPeerSyncNone);

    /* Named case C -- old firmware / this #info's Firmware: has cleared diag_*.
     * An all-zero SrFirmwareInfo reads as st=0, which would look like a confident
     * IDLE; diag_seen is what separates the two. Both st=0 and st=1 must Wait
     * (keep pending) while diag_seen is false — None would one-shot the ask (C1). */
    CHECK(sr_peer_sync_eval(false, 0u, SrSessionIdle, false) == SrPeerSyncWait);
    CHECK(sr_peer_sync_eval(false, 1u, SrSessionIdle, false) == SrPeerSyncWait);

    /* Named case D -- second Dash entry inside one launch. Already Running, so the
     * reply must not reset the statistics. */
    CHECK(sr_peer_sync_eval(true, 1u, SrSessionRunning, false) == SrPeerSyncNone);

    /* Named case E -- the three refusing states this card deliberately does not adopt
     * (scope decided by the user 2026-09-09). If a later card widens the scope, these
     * four CHECKs are the ones to change, on purpose. */
    CHECK(sr_peer_sync_eval(true, 2u, SrSessionIdle, false) == SrPeerSyncNone);
    CHECK(sr_peer_sync_eval(true, 3u, SrSessionIdle, false) == SrPeerSyncNone);
    CHECK(sr_peer_sync_eval(true, 5u, SrSessionIdle, false) == SrPeerSyncNone);
    CHECK(sr_peer_sync_eval(true, 4u, SrSessionIdle, false) == SrPeerSyncNone);

    /* Named: sr_peer_sync_on_tick keep/disarm. NC-B lands on the keep=true CHECK.
     * rev moved but Diag: of this #info not seen → Wait, pending stays.
     * A Wait that also disarms is C1 still open. */
    {
        SrPeerSyncTick t;

        t = sr_peer_sync_on_tick(
            true, 1u, 0u, false, 1u, SrSessionIdle, false);
        CHECK(t.keep_pending == true);
        CHECK(t.act == SrPeerSyncWait);

        t = sr_peer_sync_on_tick(
            true, 0u, 0u, true, 1u, SrSessionIdle, false);
        CHECK(t.keep_pending == true);
        CHECK(t.act == SrPeerSyncNone); /* no firmware_rev movement yet */

        t = sr_peer_sync_on_tick(
            true, 1u, 0u, true, 1u, SrSessionIdle, false);
        CHECK(t.keep_pending == false);
        CHECK(t.act == SrPeerSyncAdoptRunning);

        t = sr_peer_sync_on_tick(
            true, 1u, 0u, true, 0u, SrSessionIdle, false);
        CHECK(t.keep_pending == false);
        CHECK(t.act == SrPeerSyncNone); /* seen, not SCANNING */

        t = sr_peer_sync_on_tick(
            false, 1u, 0u, false, 1u, SrSessionIdle, false);
        CHECK(t.keep_pending == false);
        CHECK(t.act == SrPeerSyncNone);
    }

    /* ---- sr_model_adopt_running ---- */

    CHECK(sr_model_adopt_running(NULL, 1234u) == false);

    /* From Idle: same transition an SrEventScanStarted takes. */
    model_fresh(&m);
    CHECK(m.session == SrSessionIdle);
    CHECK(m.session_rev == 0u);
    CHECK(sr_model_adopt_running(&m, 7000u) == true);
    CHECK(m.session == SrSessionRunning);
    CHECK(m.started_tick_ms == 7000u);
    CHECK(m.session_rev == 1u);
    CHECK(m.illegal_trans == 0u);

    /* Already Running: apply_started's illegal path. sr_peer_sync_eval never lets this
     * happen; pinning it means a future caller that skips the guard is visible in
     * illegal_trans instead of silently resetting a live session's counters. */
    m.ap_wifi = 9u;
    CHECK(sr_model_adopt_running(&m, 8000u) == false);
    CHECK(m.session == SrSessionRunning);
    CHECK(m.illegal_trans == 1u);
    CHECK(m.started_tick_ms == 7000u); /* untouched */
    CHECK(m.ap_wifi == 9u);            /* untouched */
    CHECK(m.session_rev == 1u);        /* untouched */

    /* From Stopped: the previous session's statistics must not carry over. This is the
     * reason adopt reuses apply_started rather than assigning the three fields. */
    model_fresh(&m);
    m.session = SrSessionStopped;
    m.ap_wifi = 11u;
    m.ap_ble = 5u;
    m.unique_est = 13u;
    m.with_gps_fix = 3u;
    m.gps_csv_rev = 4u;
    m.session_rev = 6u;
    CHECK(sr_model_adopt_running(&m, 9000u) == true);
    CHECK(m.session == SrSessionRunning);
    CHECK(m.started_tick_ms == 9000u);
    CHECK(m.session_rev == 7u); /* cumulative, reset must not clear it (ADR-017 d3) */
    CHECK(m.ap_wifi == 0u);
    CHECK(m.ap_ble == 0u);
    CHECK(m.unique_est == 0u);
    CHECK(m.with_gps_fix == 0u);
    CHECK(m.gps_csv_rev == 0u);

    printf(
        "peer_sync cover: total=%u adopt=%u none=%u adopt_at_st1=%u\n",
        cov_total,
        cov_adopt,
        cov_none,
        cov_state[1]);

    return sr_test_failures;
}
