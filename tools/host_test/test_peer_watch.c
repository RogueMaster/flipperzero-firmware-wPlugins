#include "sr_test.h"

#include "sr_peer_watch.h"

#include <string.h>

/* One info reply every 5 s, same as SR_QUAL_REFRESH_PERIOD_TICKS. */
#define REPLY_MS 5000u

typedef struct {
    SrPeerWatchCtx c;
    SrPeerWatchIn in;
} Rig;

static void rig_init(Rig* r, uint32_t now) {
    sr_peer_watch_init(&r->c);
    memset(&r->in, 0, sizeof(r->in));
    r->in.sigroam = true;
    r->in.session = SrSessionRunning;
    r->in.session_rev = 1u;
    r->in.sess_rev = 1u;
    r->in.sess_ms = 1000u;
    r->in.diag_seen = true;
    r->in.diag_state = 1u;
    r->in.now_ms = now;
    /* First eval only primes. */
    CHECK(sr_peer_watch_eval(&r->c, &r->in) == SrPeerWatchActNone);
}

static SrPeerWatchAct reply(Rig* r, uint32_t now, uint8_t st, uint32_t ms) {
    r->in.now_ms = now;
    r->in.sess_rev++;
    r->in.diag_state = st;
    r->in.sess_ms = ms;
    return sr_peer_watch_eval(&r->c, &r->in);
}

static SrPeerWatchAct tick(Rig* r, uint32_t now) {
    r->in.now_ms = now;
    return sr_peer_watch_eval(&r->c, &r->in);
}

/* What sigroam.c does on Restart: mark the model Stopped, queue wardrive. */
static void caller_restart(Rig* r, uint32_t now) {
    r->in.session = SrSessionStopped;
    r->in.session_rev++;
    sr_peer_watch_note_sent(&r->c, now, r->in.session_rev);
}

static void board_accepts(Rig* r, uint32_t now) {
    r->in.session = SrSessionRunning;
    r->in.session_rev++;
    r->in.now_ms = now;
}

/* 2026-09-24 17:59:10: USB plug-in reset the ESP, FAP stayed Running. */
static void test_path_b_reboot(void) {
    Rig r;
    uint32_t t;

    rig_init(&r, 0u);
    for(t = REPLY_MS; t <= 30000u; t += REPLY_MS) {
        CHECK(reply(&r, t, 1u, t) == SrPeerWatchActNone);
    }
    CHECK(sr_peer_watch_hint(&r.c) == SR_RESYNC_HINT_NONE);

    /* Board back from reset: IDLE, no session. First bad reply only suspects. */
    CHECK(reply(&r, 35000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(r.c.phase == SrPeerWatchIdle);
    CHECK(reply(&r, 40000u, 0u, 0u) == SrPeerWatchActRestart);
    CHECK(r.c.phase == SrPeerWatchRecovering);
    CHECK(sr_peer_watch_hint(&r.c) == SR_RESYNC_HINT_BUSY);

    caller_restart(&r, 40000u);
    CHECK(tick(&r, 40100u) == SrPeerWatchActNone);

    board_accepts(&r, 40200u);
    CHECK(sr_peer_watch_eval(&r.c, &r.in) == SrPeerWatchActNone);
    CHECK(r.c.phase == SrPeerWatchIdle);
    CHECK(sr_peer_watch_hint(&r.c) == SR_RESYNC_HINT_NONE);

    /* New session starts its own grace: early IDLE replies are ignored. */
    CHECK(reply(&r, 45000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 50000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 60000u, 1u, 100u) == SrPeerWatchActNone);
}

/* 2026-09-24 16:38: unplug, board kept SCANNING. Nothing may be sent. */
static void test_path_a_no_reboot(void) {
    Rig r;
    uint32_t t;

    CHECK(sr_peer_watch_legacy_resync_allowed(true) == false);
    CHECK(sr_peer_watch_legacy_resync_allowed(false) == true);

    rig_init(&r, 0u);
    for(t = REPLY_MS; t <= 600000u; t += REPLY_MS) {
        CHECK(reply(&r, t, 1u, t) == SrPeerWatchActNone);
    }
    CHECK(r.c.phase == SrPeerWatchIdle);
}

/* Seal (~34 s) + upload (~28 s): Busy until IDLE, retried every 5 s. */
static void test_busy_window_retry(void) {
    Rig r;
    uint32_t t;
    uint32_t sends = 0u;
    SrPeerWatchAct a;

    rig_init(&r, 0u);
    CHECK(reply(&r, 20000u, 2u, 900u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 3u, 900u) == SrPeerWatchActRestart);
    caller_restart(&r, 25000u);

    for(t = 25100u; t < 25000u + 62000u; t += 100u) {
        a = tick(&r, t);
        CHECK(a != SrPeerWatchActRestart);
        if(a == SrPeerWatchActSendStart) {
            sends++;
            sr_peer_watch_note_sent(&r.c, t, r.in.session_rev);
        }
    }
    /* One resend per 5 s gap, never per tick. */
    CHECK(sends >= 11u && sends <= 13u);
    CHECK(r.c.phase == SrPeerWatchRecovering);

    board_accepts(&r, 25000u + 62000u);
    CHECK(sr_peer_watch_eval(&r.c, &r.in) == SrPeerWatchActNone);
    CHECK(r.c.phase == SrPeerWatchIdle);
}

/* A failed queue must not wait a full gap before trying again. */
static void test_queue_failed(void) {
    Rig r;

    rig_init(&r, 0u);
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 0u) == SrPeerWatchActRestart);
    /* Caller marked Stopped but the worker slot was full: no note_sent. */
    r.in.session = SrSessionStopped;
    r.in.session_rev++;
    CHECK(tick(&r, 25100u) == SrPeerWatchActSendStart);
    sr_peer_watch_note_sent(&r.c, 25100u, r.in.session_rev);
    CHECK(tick(&r, 25200u) == SrPeerWatchActNone);
}

static void test_giveup(void) {
    Rig r;
    uint32_t t;
    SrPeerWatchAct a;

    rig_init(&r, 0u);
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 0u) == SrPeerWatchActRestart);
    caller_restart(&r, 25000u);

    for(t = 25100u; t < 25000u + (uint32_t)SR_PEER_WATCH_GIVEUP_MS; t += 100u) {
        a = tick(&r, t);
        if(a == SrPeerWatchActSendStart) {
            sr_peer_watch_note_sent(&r.c, t, r.in.session_rev);
        }
    }
    CHECK(tick(&r, 25000u + (uint32_t)SR_PEER_WATCH_GIVEUP_MS) == SrPeerWatchActNone);
    CHECK(r.c.phase == SrPeerWatchLost);
    CHECK(sr_peer_watch_hint(&r.c) == SR_RESYNC_HINT_LOST);
    CHECK(tick(&r, 25000u + (uint32_t)SR_PEER_WATCH_GIVEUP_MS + 60000u) == SrPeerWatchActNone);

    /* Operator presses OK, board accepts: back to watching. */
    board_accepts(&r, 400000u);
    CHECK(sr_peer_watch_eval(&r.c, &r.in) == SrPeerWatchActNone);
    CHECK(r.c.phase == SrPeerWatchIdle);
    CHECK(sr_peer_watch_hint(&r.c) == SR_RESYNC_HINT_NONE);
}

static void test_no_false_trigger(void) {
    Rig r;
    int i;

    /* One bad reply between good ones. */
    rig_init(&r, 0u);
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 1u, 25000u) == SrPeerWatchActNone);
    CHECK(reply(&r, 30000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 35000u, 1u, 35000u) == SrPeerWatchActNone);

    /* Inside the grace after Running began. */
    rig_init(&r, 0u);
    CHECK(reply(&r, 5000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 10000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 14999u, 0u, 0u) == SrPeerWatchActNone);

    /* The same reply re-read on every GUI tick counts once. */
    rig_init(&r, 0u);
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    for(i = 0; i < 50; i++) {
        CHECK(tick(&r, 20100u + (uint32_t)i * 100u) == SrPeerWatchActNone);
    }

    /* Operator command in flight. */
    rig_init(&r, 0u);
    r.in.cmd_pending = true;
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 30000u, 0u, 0u) == SrPeerWatchActNone);

    /* Not Running: nothing to reconcile. */
    rig_init(&r, 0u);
    r.in.session = SrSessionStopped;
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 0u) == SrPeerWatchActNone);

    /* Generic Marauder: never. */
    rig_init(&r, 0u);
    r.in.sigroam = false;
    CHECK(reply(&r, 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 30000u, 0u, 0u) == SrPeerWatchActNone);

    /* No Diag line: only ms==0 counts. */
    rig_init(&r, 0u);
    r.in.diag_seen = false;
    CHECK(reply(&r, 20000u, 0u, 20000u) == SrPeerWatchActNone);
    CHECK(reply(&r, 25000u, 0u, 25000u) == SrPeerWatchActNone);
    CHECK(reply(&r, 30000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, 35000u, 0u, 0u) == SrPeerWatchActRestart);
}

static void test_wrap(void) {
    Rig r;
    uint32_t base = 0xFFFFF000u;

    rig_init(&r, base);
    CHECK(reply(&r, base + 5000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, base + 10000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, base + 20000u, 0u, 0u) == SrPeerWatchActNone);
    CHECK(reply(&r, base + 25000u, 0u, 0u) == SrPeerWatchActRestart);
}

static void test_null(void) {
    SrPeerWatchCtx c;
    SrPeerWatchIn in;

    memset(&in, 0, sizeof(in));
    sr_peer_watch_init(NULL);
    sr_peer_watch_init(&c);
    CHECK(sr_peer_watch_eval(NULL, &in) == SrPeerWatchActNone);
    CHECK(sr_peer_watch_eval(&c, NULL) == SrPeerWatchActNone);
    sr_peer_watch_note_sent(NULL, 0u, 0u);
    CHECK(sr_peer_watch_hint(NULL) == SR_RESYNC_HINT_NONE);
}

int test_peer_watch_run(void) {
    sr_test_failures = 0;
    test_path_b_reboot();
    test_path_a_no_reboot();
    test_busy_window_retry();
    test_queue_failed();
    test_giveup();
    test_no_false_trigger();
    test_wrap();
    test_null();
    return sr_test_failures;
}
