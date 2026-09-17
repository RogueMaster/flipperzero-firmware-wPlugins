#include "sr_test.h"

#include "sr_sess_seed.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"
#include "sr_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SrBloom g_bloom;
static SrRawLog g_rawlog;

static void model_fresh(SrModel* m) {
    memset(m, 0, sizeof(*m));
    sr_bloom_init(&g_bloom);
    memset(&g_rawlog, 0, sizeof(g_rawlog));
    sr_model_init(m, &g_bloom, &g_rawlog);
}

int test_sess_seed_run(void);

int test_sess_seed_run(void) {
    SrModel m;
    SrEvent ev;

    CHECK((unsigned)SrEventUnknown == 7u);
    CHECK((unsigned)SrEventBusy == 8u);
    CHECK((unsigned)SrEventSess == 9u);

    /* Guard 1 — local ScanStarted session is not seeded. */
    CHECK(
        sr_sess_seed_eval(1u, 0u, 201270u, SrSessionRunning, false) == SrSessSeedNone);

    /* Named: a missing Sess: line is not written as 0 (contract 2). NC4 lands here. sess_ms is non-zero so
     * the ms==0 guard cannot mask a missing sess_rev==0 check. */
    CHECK(
        sr_sess_seed_eval(0u, 0u, 201270u, SrSessionRunning, true) == SrSessSeedNone);

    /* Named: ms=0 does not seed (contract 1). NC3 lands here. sess_rev moved so
     * the absence / at_send guards cannot mask a missing ms==0 check. */
    CHECK(sr_sess_seed_eval(1u, 0u, 0u, SrSessionRunning, true) == SrSessSeedNone);

    /* Named: Diag: st=1 with Sess: ms=0 is a contradiction; do not seed. Same inputs as the
     * ms=0 guard; kept as its own CHECK so the combination is named. */
    CHECK(
        sr_sess_seed_eval(1u, 0u, 0u, SrSessionRunning, true) == SrSessSeedNone);

    /* Guard 4 — seed only an already-adopted live session. */
    CHECK(sr_sess_seed_eval(1u, 0u, 201270u, SrSessionIdle, true) == SrSessSeedNone);
    CHECK(
        sr_sess_seed_eval(1u, 0u, 201270u, SrSessionStopped, true) == SrSessSeedNone);

    /* Named: Sess: arrives after the claim. NC5 lands on the Apply CHECK.
     * Cold Dash: at_send=0. Claim fires while sess_rev is still 0
     * (Firmware:/Diag: arrived first); pending stays set; the Sess: line
     * bumps sess_rev to 1. Binding the seed to the firmware_rev one-shot
     * would clear pending on that first None and the second call would
     * never Apply. */
    CHECK(
        sr_sess_seed_eval(0u, 0u, 201270u, SrSessionRunning, true) == SrSessSeedNone);
    CHECK(
        sr_sess_seed_eval(1u, 0u, 201270u, SrSessionRunning, true) == SrSessSeedApply);

    /* Named: Probe leftover must not seed. NC-C lands here.
     * at_send=4 is the snapshot taken before Dash queued info; sess_rev
     * is still 4 because this #info's Sess: has not arrived. sess_rev != 0
     * alone would Apply with stale AP/BLE/elapsed (C3). */
    CHECK(
        sr_sess_seed_eval(4u, 4u, 201270u, SrSessionRunning, true) == SrSessSeedNone);

    /* Named: Dash's own Sess: arrived after Probe. at_send=4, rev=5. */
    CHECK(
        sr_sess_seed_eval(5u, 4u, 201270u, SrSessionRunning, true) == SrSessSeedApply);

    /* After a successful seed the caller clears pending; further ticks are None. */
    CHECK(
        sr_sess_seed_eval(1u, 0u, 201270u, SrSessionRunning, false) == SrSessSeedNone);

    /* ---- sr_model_apply SrEventSess: store snapshot, bump sess_rev, not unknown ---- */

    CHECK(sr_model_seed_from_sess(NULL, 1u) == false);

    model_fresh(&m);
    CHECK(m.sess_rev == 0u);
    CHECK(m.unknown_lines == 0u);
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventSess;
    ev.u.sess.ap = 1288u;
    ev.u.sess.ble = 8171u;
    ev.u.sess.ms = 201270u;
    CHECK(sr_model_apply(&m, &ev, 100u) == true);
    CHECK(m.sess_rev == 1u);
    CHECK(m.sess.ap == 1288u);
    CHECK(m.sess.ble == 8171u);
    CHECK(m.sess.ms == 201270u);
    CHECK(m.unknown_lines == 0u);
    CHECK(m.ap_wifi == 0u); /* apply stores the snapshot; it does not seed */

    /* reset_session keeps sess / sess_rev (peer snapshot, not session data). */
    m.ap_wifi = 9u;
    sr_model_reset_session(&m, false);
    CHECK(m.sess_rev == 1u);
    CHECK(m.sess.ap == 1288u);
    CHECK(m.sess.ms == 201270u);
    CHECK(m.ap_wifi == 0u);

    /* ---- sr_model_seed_from_sess ---- */

    /* Named: Flipper tick is less than session ms. NC6 lands here.
     * now=5000, ms=201270 → started wraps; elapsed must still be 201270.
     * A clamp of `if(ms > now) started = 0` would make elapsed == now. */
    model_fresh(&m);
    CHECK(sr_model_adopt_running(&m, 5000u) == true);
    m.sess.ap = 1288u;
    m.sess.ble = 8171u;
    m.sess.ms = 201270u;
    CHECK(sr_model_seed_from_sess(&m, 5000u) == true);
    CHECK(m.started_tick_ms == (5000u - 201270u));
    CHECK((5000u - m.started_tick_ms) == 201270u);
    CHECK(m.ap_wifi == 1288u);
    CHECK(m.ap_ble == 8171u);

    /* Named: CSV rows arrive after claim, then seed overwrites rather than adds. NC7 lands here.
     * The three WiFi rows are already inside sess.ap; += would yield 1291. */
    model_fresh(&m);
    CHECK(sr_model_adopt_running(&m, 7000u) == true);
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventApFound;
    memcpy(ev.u.ap.bssid, "AA:BB:CC:DD:EE:01", 18);
    CHECK(sr_model_apply(&m, &ev, 7001u) == true);
    memcpy(ev.u.ap.bssid, "AA:BB:CC:DD:EE:02", 18);
    CHECK(sr_model_apply(&m, &ev, 7002u) == true);
    memcpy(ev.u.ap.bssid, "AA:BB:CC:DD:EE:03", 18);
    CHECK(sr_model_apply(&m, &ev, 7003u) == true);
    CHECK(m.ap_wifi == 3u);
    m.sess.ap = 1288u;
    m.sess.ble = 8171u;
    m.sess.ms = 201270u;
    CHECK(sr_model_seed_from_sess(&m, 8000u) == true);
    CHECK(m.ap_wifi == 1288u);
    CHECK(m.ap_ble == 8171u);

    printf("sess_seed named: eval+seed ok\n");
    return sr_test_failures;
}
