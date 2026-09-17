#define SR_HOST_TEST 1

#include "sr_test.h"

#include "sr_capture_health.h"
#include "sr_view_fmt.h"
#include "sr_types.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"
#include "sr_peer_sync.h"
#include "sr_sess_seed.h"
#include "../../views/sr_view_dash.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * F2 rev2 (docs/exec-plans/f2-capture-health-rev2.md, FAP side of the frozen
 * card). Six use cases from card §6, in order. Each comment below names
 * which §6 item and which negative control (NC-A/B/C) it locks.
 *
 * Use case 5 (debug_rows hides the health row, heap row survives) has no
 * pure-function home: the real decision lives inside
 * views/sr_view_dash.c's sr_view_dash_draw_dash(), which needs a live
 * Canvas* and cannot run on host. Its real discriminating power lives in
 * the Makefile's dash_health_guard target (grep on the actual production
 * source), wired into `make -C tools/host_test all`. Faking an
 * independent re-derivation of the row arithmetic here would not actually
 * exercise the fix and was deliberately left out (see the repo's own
 * "oracle independence" convention: re-deriving the same formula the fix
 * uses is not evidence the fix is present).
 */

static int streq(const char* a, const char* b) {
    if(a == NULL || b == NULL) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

static SrQualInfo qual_ok(void) {
    SrQualInfo q;
    memset(&q, 0, sizeof(q));
    q.gga = 100u;
    q.ggafix = 100u;
    q.drop = 0u;
    q.net = 142u;
    q.sd = 1u;
    q.sats = 8u;
    q.topd = 0u;
    return q;
}

/*
 * §6 use case 1 / NC-B (part 1): periodic re-send must not touch
 * re-adopt or re-seed.
 *
 * Part A: model level. A real session is started the normal way, a real
 * wardrive row bumps ap_wifi, then 3 consecutive "#info replies" (a Sess:
 * line and a Qual: line each) are fed through sr_model_apply alone --
 * exactly the shape of what the periodic dash_qual_refresh_tick path
 * produces, since that path never calls sr_model_adopt_running /
 * sr_model_seed_from_sess. session_rev and ap_wifi must not move.
 *
 * Part B: the guard functions the hard constraint leans on
 * (sr_peer_sync.h / sr_sess_seed.h, both frozen, not part of this
 * change). As long as pending stays false -- which is exactly what the
 * new scene_dash.c code must never violate -- both must answer None on
 * every one of the 3 replies, even though fw_rev / sess_rev genuinely
 * rise and Diag: genuinely reports SCANNING each time.
 */
static void t_periodic_refresh_no_readopt_reseed(void) {
    SrBloom bloom;
    SrRawLog rawlog;
    SrModel m;
    SrEvent ev;
    uint32_t ap0;
    uint32_t session_rev0;
    int i;
    uint32_t fw_rev;
    uint32_t srev;

    sr_bloom_init(&bloom);
    memset(&rawlog, 0, sizeof(rawlog));
    sr_model_init(&m, &bloom, &rawlog);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventScanStarted;
    CHECK(sr_model_apply(&m, &ev, 1000u));
    CHECK(m.session == SrSessionRunning);
    session_rev0 = m.session_rev;
    CHECK(session_rev0 == 1u);

    {
        SrApRecord rec;
        memset(&rec, 0, sizeof(rec));
        sr_strlcpy(rec.bssid, sizeof(rec.bssid), "AA:BB:CC:DD:EE:01");
        sr_strlcpy(rec.ssid, sizeof(rec.ssid), "Real");
        rec.rssi = -60;
        rec.channel = 6;
        rec.radio = SrRadioWifi;
        memset(&ev, 0, sizeof(ev));
        ev.kind = SrEventApFound;
        ev.u.ap = rec;
        CHECK(sr_model_apply(&m, &ev, 2000u));
    }
    ap0 = m.ap_wifi;
    CHECK(ap0 == 1u);

    for(i = 0; i < 3; i++) {
        uint32_t tick = 3000u + (uint32_t)i * 5000u;

        memset(&ev, 0, sizeof(ev));
        ev.kind = SrEventSess;
        ev.u.sess.ap = 900u + (uint32_t)i; /* board's own totals; must NOT land in ap_wifi */
        ev.u.sess.ble = 700u + (uint32_t)i;
        ev.u.sess.ms = 60000u + (uint32_t)i;
        CHECK(sr_model_apply(&m, &ev, tick));

        memset(&ev, 0, sizeof(ev));
        ev.kind = SrEventQual;
        ev.u.qual.gga = 10u + (uint32_t)i;
        ev.u.qual.ggafix = 10u + (uint32_t)i;
        ev.u.qual.drop = 0u;
        ev.u.qual.net = 1u;
        ev.u.qual.sd = 1u;
        CHECK(sr_model_apply(&m, &ev, tick));

        CHECK(m.session_rev == session_rev0);
        CHECK(m.session == SrSessionRunning);
        CHECK(m.ap_wifi == ap0);
        CHECK(m.sess_rev == (uint32_t)(i + 1));
        CHECK(m.sess.ap == 900u + (uint32_t)i); /* the snapshot itself did update */
        CHECK(m.qual_tick_ms == tick); /* prerequisite for use case 2: bumped on every Qual */
    }

    fw_rev = 5u;
    srev = 5u;
    for(i = 0; i < 3; i++) {
        SrPeerSyncTick t;
        SrSessSeedAct sa;

        fw_rev++;
        t = sr_peer_sync_on_tick(
            false /* pending: the periodic path must never set this true */,
            fw_rev,
            fw_rev - 1u,
            true /* diag_seen */,
            (uint8_t)SrPeerStScanning,
            SrSessionRunning,
            false /* cmd_pending */);
        CHECK(t.keep_pending == false);
        CHECK(t.act == SrPeerSyncNone);

        srev++;
        sa =
            sr_sess_seed_eval(srev, srev - 1u, 60000u, SrSessionRunning, false /* seed_pending */);
        CHECK(sa == SrSessSeedNone);
    }
}

/*
 * §6 use case 1 (part 2) / NC-B (part 2): "Qual value advances over time".
 * Drives the pure periodicity decision (sr_qual_refresh_due, mirrors
 * scene_dash.c's dash_qual_refresh_tick gate) across 500 simulated 100 ms
 * ticks (50 s). Each time a refresh is judged due, pretend the periodic
 * #info's Qual: landed right then. With true periodicity (every 50 ticks =
 * 5000 ms, card §1A's T) the headline must never go stale, since
 * SR_QUAL_STALE_MS=15000 is 3x the period. NC-B breaks the due-check into
 * "fire once" and this must go red.
 */
static void t_refresh_due_keeps_headline_fresh(void) {
    uint32_t period_ticks;
    uint32_t tick_n;
    uint32_t qual_tick_ms = 0u;
    bool ever_stale = false;

    /* E-2: pin the production period, then drive due-checks from it.
     * Mirroring a magic 50u here would stay green if production moved. */
    CHECK((int)SR_QUAL_REFRESH_PERIOD_TICKS == 50);
    period_ticks = (uint32_t)SR_QUAL_REFRESH_PERIOD_TICKS;

    CHECK(sr_qual_refresh_due(0u, period_ticks) == true);
    CHECK(sr_qual_refresh_due(1u, period_ticks) == false);
    CHECK(sr_qual_refresh_due(period_ticks - 1u, period_ticks) == false);
    CHECK(sr_qual_refresh_due(period_ticks, period_ticks) == true);
    CHECK(sr_qual_refresh_due(period_ticks, 0u) == false); /* period 0 never fires (defensive) */

    for(tick_n = 0; tick_n <= 500u; tick_n++) {
        uint32_t now_ms = tick_n * 100u;
        if(sr_qual_refresh_due(tick_n, period_ticks)) {
            qual_tick_ms = now_ms;
        }
        if(!sr_fmt_qual_fresh(1u /* qual_rev */, qual_tick_ms, 1u /* sess_ms */, now_ms)) {
            ever_stale = true;
        }
    }
    CHECK(ever_stale == false);
}

/*
 * §6 use case 2 / NC-A: superannuated Qual degrades to the unknown state.
 * At exactly 3xT (SR_QUAL_STALE_MS) it is still shown (card §1B: two missed
 * refreshes are allowed); one ms past, it must not be. NC-A flips the comparison to
 * "always fresh" and this must go red on the second CHECK.
 */
static void t_stale_qual_degrades(void) {
    SrQualInfo q = qual_ok();
    SrHealthEval hv = sr_capture_health_eval(&q, 120000u);
    char health[40];
    bool fresh_at_bound;
    bool fresh_past_bound;
    size_t n;

    CHECK(hv.v == SrHealthOk);

    /* Oracle independence: the two calls below build their inputs FROM
     * SR_QUAL_STALE_MS, so they pin the boundary semantics (> not >=) but go
     * green for ANY value of the constant. Card §1B fixes the value at 15000
     * (3xT, two missed refreshes allowed), so pin the value itself too -- same shape as
     * t_headline_over_20_cols_fits pinning SR_HEALTH_COLS_MAX == 32. */
    CHECK((int)SR_QUAL_STALE_MS == 15000);

    fresh_at_bound = sr_fmt_qual_fresh(
        1u /* qual_rev */,
        1000u /* qual_tick_ms */,
        5000u /* sess_ms */,
        1000u + (uint32_t)SR_QUAL_STALE_MS);
    CHECK(fresh_at_bound == true);

    fresh_past_bound =
        sr_fmt_qual_fresh(1u, 1000u, 5000u, 1000u + (uint32_t)SR_QUAL_STALE_MS + 1u);
    CHECK(fresh_past_bound == false);

    n = sr_view_fmt_health(&hv, &q, fresh_past_bound, health, sizeof(health));
    CHECK(n > 0);
    CHECK(streq(health, "- need SigRoam Qual"));
}

/*
 * §6 use case 3: the board has no session (Sess: ms==0). Must not paint a
 * verdict even though Qual: was just seen (age 0) -- card §1C.
 */
static void t_no_board_session_suppresses_verdict(void) {
    SrQualInfo q = qual_ok();
    SrHealthEval hv = sr_capture_health_eval(&q, 120000u);
    char health[40];
    bool fresh;

    fresh =
        sr_fmt_qual_fresh(1u /* qual_rev */, 9000u /* qual_tick_ms */, 0u /* sess_ms */, 9000u);
    CHECK(fresh == false);

    (void)sr_view_fmt_health(&hv, &q, fresh, health, sizeof(health));
    CHECK(streq(health, "- need SigRoam Qual"));
}

/*
 * §6 use case 4 / NC-C: the 23-char status bar "OK fix100% 0drop SAT 08"
 * (D19 / ADR-025: `net` out, `SAT <NN>` in) must land fully inside
 * SR_HEALTH_COLS_MAX(32), no '~'. Also pins that
 * the old SR_VIEW_COLS(20) budget really would have cut it -- that is
 * exactly what NC-C reintroduces if the retreat loop's start reverts to
 * SR_VIEW_COLS (the production-source half of that regression is caught
 * by the Makefile's dash_health_guard target, since sr_fmt_fit here is
 * called with the *constant*, not by re-running the device draw code).
 */
static void t_headline_over_20_cols_fits(void) {
    SrQualInfo q = qual_ok();
    SrHealthEval hv = sr_capture_health_eval(&q, 120000u);
    char health[40];
    char line[SR_HEALTH_COLS_MAX + 1];
    char old[SR_VIEW_COLS + 1];
    size_t hn;
    size_t n;
    size_t on;

    CHECK((int)SR_HEALTH_COLS_MAX == 32);

    hn = sr_view_fmt_health(&hv, &q, true, health, sizeof(health));
    CHECK(hn == 23u);
    CHECK(streq(health, "OK fix100% 0drop SAT 08"));

    n = sr_fmt_fit(health, hn, (size_t)SR_HEALTH_COLS_MAX, line, sizeof(line));
    CHECK(n == 23u);
    CHECK(streq(line, "OK fix100% 0drop SAT 08"));
    CHECK(strchr(line, '~') == NULL);

    on = sr_fmt_fit(health, hn, (size_t)SR_VIEW_COLS, old, sizeof(old));
    CHECK(on == 20u);
    CHECK(old[19] == '~');
}

/*
 * §6 use case 6: fixpct clamp. gate item 7 (gps_task.c write-seq vs
 * store_task.c read-seq) can hand the FAP a Qual: line where ggafix > gga,
 * which the frozen sr_capture_health_eval (unclamped by design, not part
 * of this whitelist) turns into fixpct > 100. Display must clamp to 100.
 */
static void t_fixpct_clamps_for_display(void) {
    SrQualInfo q;
    SrHealthEval hv;
    char health[40];

    memset(&q, 0, sizeof(q));
    q.gga = 50u;
    q.ggafix = 60u; /* > gga */
    q.drop = 0u;
    q.net = 1u;
    q.sd = 1u;

    hv = sr_capture_health_eval(&q, 120000u);
    CHECK(hv.v == SrHealthOk);
    CHECK(hv.fixpct == 120u); /* eval layer stays unclamped, untouched by this card */

    (void)sr_view_fmt_health(&hv, &q, true, health, sizeof(health));
    CHECK(strstr(health, "fix100%") != NULL);
    CHECK(strstr(health, "fix120%") == NULL);
}

int test_f2_health2_run(void);

int test_f2_health2_run(void) {
    sr_test_failures = 0;

    t_periodic_refresh_no_readopt_reseed();
    t_refresh_due_keeps_headline_fresh();
    t_stale_qual_degrades();
    t_no_board_session_suppresses_verdict();
    t_headline_over_20_cols_fits();
    t_fixpct_clamps_for_display();

    fprintf(stderr, "f2_health2: ok\n");
    return sr_test_failures;
}
