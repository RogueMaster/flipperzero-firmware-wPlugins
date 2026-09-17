#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sr_model.h"    /* SrSessionState */
#include "sr_scan_ctl.h" /* SR_SCAN_CTL_TIMEOUT_MS */

/*
 * ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 *
 * D11-RESYNC: after a VBUS falling edge while the app still thinks it is
 * scanning, wait until the peer speaks again, then recover with the same
 * two-command sequence the operator already uses by hand
 * (stopscan → wardrive). Confirmation reuses sr_scan_ctl (session_rev rose
 * plus the expected SrSessionState). The GUI thread is the only sender.
 *
 * 24 / 5000 / 150000 (2026-09-03 continuation 41, user decision). Unlike the earlier
 * 3 / 5000 / 30000, these are NOT pulled out of thin air -- but read the split carefully:
 *
 *   MEASURED: the peer needs (67, 72] s from power loss to answering a stopscan.
 *     Two independent runs both showed delta tx = 144 B = 16 * 9 B, i.e. try 15 (fired at
 *     2 + 14*5 = 72 s) was the first one confirmed, while try 14 at 67 s still failed.
 *     Evidence: artifacts/xu43- and xu44-d11-diag16-*-20260903.log, plus the on-screen
 *     AP=22 BLE=17 uniq=12 @ 00:36 that proved the scan itself came back.
 *   PRODUCT CHOICE: how much headroom to leave on top of that. 24 tries covers
 *     2 + 23*5 = 117 s, which is ~1.6x the measured 72 s. The reason for wanting that much:
 *     the same two runs each received ~2 banners' worth of bytes while unplugged, which
 *     suggests the peer reboots TWICE (never measured directly), so a third reboot would
 *     push the number well past 77 s. GIVEUP 150000 keeps the giveup strictly outside the
 *     retry window (117 s) with room for the two-step confirm.
 *
 * ⛔ Do not describe the headroom as measured, and do not describe the 72 s as a stable
 * constant: it is two samples of something with reboot-count and banner-chunking jitter.
 *
 * 1000 (SR_RESYNC_RX_QUIET_MS) is also a product choice: after rx_bytes has
 * grown, wait until it stops growing for this long before the first stopscan.
 * It is not a sleep from the VBUS edge (the card forbids that). 2026-09-03
 * field run: +1 byte was enough to fire while the boot banner was still
 * trickling (Δ 411–613 B vs 1297 B, Lost at ~15 s, session still Running).
 *
 * 2026-09-03 C: after stopscan is confirmed (Stopped), do not return to
 * NeedStop. AwaitStart waits for Running or the 30000 giveup; it does not
 * use SR_SCAN_CTL_TIMEOUT_MS. Retrying stopscan after a slow wardrive was
 * killing the scan (Lost at ~14 s, then rx kept growing >10 s later).
 * 3 / 5000 / 2000 still apply to the stopscan step only.
 */

enum {
    SR_RESYNC_MAX_TRIES = 24,
    SR_RESYNC_RETRY_GAP_MS = 5000,
    SR_RESYNC_GIVEUP_MS = 150000,
    SR_RESYNC_RX_QUIET_MS = 1000
};

_Static_assert(SR_RESYNC_RX_QUIET_MS > 0u, "quiet gap must be positive");
_Static_assert(
    SR_RESYNC_RX_QUIET_MS < SR_RESYNC_GIVEUP_MS, "quiet gap must fit inside giveup");

/*
 * Packed into SrDashModel.wait_stage (uint8_t). Not SrWaitStage values.
 * SrDashModel cannot grow (test_gps_sample.c pins sizeof == 644).
 */
enum {
    SR_RESYNC_HINT_NONE = 0,
    SR_RESYNC_HINT_BUSY = 4, /* "Resyncing..." */
    SR_RESYNC_HINT_LOST = 5  /* "Scan lost, press OK" */
};

_Static_assert(sizeof("Resyncing...") - 1u <= 20u, "Resyncing... exceeds SR_VIEW_COLS");
_Static_assert(
    sizeof("Scan lost, press OK") - 1u <= 20u, "Scan lost, press OK exceeds SR_VIEW_COLS");

typedef enum {
    SrResyncIdle = 0,
    SrResyncWaitPeer,  /* VBUS edge; wait for rx growth, then Q ms with no further growth */
    SrResyncNeedStop,  /* GUI should send stopscan */
    SrResyncAwaitStop, /* stopscan in flight */
    SrResyncNeedStart, /* GUI should send wardrive */
    SrResyncAwaitStart, /* wardrive in flight */
    SrResyncLost       /* gave up; hint stays until the operator recovers */
} SrResyncPhase;

typedef enum {
    SrResyncActNone = 0,
    SrResyncActSendStop,
    SrResyncActSendStart
} SrResyncAct;

typedef struct {
    SrResyncPhase phase;
    uint8_t tries; /* sequences whose stopscan was actually queued; product cap 3 */
    bool vbus_was;
    bool vbus_seen;
    uint32_t trigger_ms;
    uint32_t try_ms; /* now_ms of this sequence's stopscan; product gap 5000 */
    uint32_t cmd_ms;
    uint32_t rx_at_trig;
    uint32_t rx_last; /* last rx_bytes seen in WaitPeer; starts equal to rx_at_trig */
    uint32_t rx_grow_ms; /* now_ms of the last rx_bytes increase in WaitPeer */
    uint32_t rev_at_trig;
    uint32_t rev_at_step2; /* session_rev snapshot taken when wardrive was queued */
} SrResyncCtx;

typedef struct {
    bool vbus_present;
    SrSessionState session;
    uint32_t session_rev;
    uint32_t rx_bytes;
    uint32_t now_ms;
} SrResyncIn;

static inline void sr_resync_init(SrResyncCtx* c) {
    if(c == NULL) {
        return;
    }
    memset(c, 0, sizeof(*c));
}

static inline void sr_resync_go_idle(SrResyncCtx* c) {
    bool was;
    bool seen;

    if(c == NULL) {
        return;
    }
    was = c->vbus_was;
    seen = c->vbus_seen;
    memset(c, 0, sizeof(*c));
    c->vbus_was = was;
    c->vbus_seen = seen;
    c->phase = SrResyncIdle;
}

static inline bool sr_resync_elapsed_ge(uint32_t now_ms, uint32_t then_ms, uint32_t dur_ms) {
    /* Unsigned subtraction: furi_get_tick overflows. Same rule as sr_scan_ctl.h:40-41. */
    return (uint32_t)(now_ms - then_ms) >= dur_ms;
}

static inline bool sr_resync_recovered(const SrResyncCtx* c, const SrResyncIn* in) {
    if(c == NULL || in == NULL) {
        return false;
    }
    return in->session == SrSessionRunning &&
           (uint32_t)(in->session_rev - c->rev_at_trig) > 0u;
}

static inline uint8_t sr_resync_hint_stage(const SrResyncCtx* c) {
    if(c == NULL || c->phase == SrResyncIdle) {
        return (uint8_t)SR_RESYNC_HINT_NONE;
    }
    if(c->phase == SrResyncLost) {
        return (uint8_t)SR_RESYNC_HINT_LOST;
    }
    return (uint8_t)SR_RESYNC_HINT_BUSY;
}

static inline void
    sr_resync_note_sent(SrResyncCtx* c, uint32_t now_ms, bool is_start, uint32_t session_rev_at_send) {
    if(c == NULL) {
        return;
    }
    c->cmd_ms = now_ms;
    if(is_start) {
        c->phase = SrResyncAwaitStart;
        c->rev_at_step2 = session_rev_at_send;
        return;
    }
    c->phase = SrResyncAwaitStop;
    if(c->tries < 255u) {
        c->tries++;
    }
    c->try_ms = now_ms;
}

static inline SrResyncAct sr_resync_eval(SrResyncCtx* c, const SrResyncIn* in) {
    bool falling;
    unsigned hops;

    if(c == NULL || in == NULL) {
        return SrResyncActNone;
    }

    falling = c->vbus_seen && c->vbus_was && !in->vbus_present;
    c->vbus_was = in->vbus_present;
    c->vbus_seen = true;

    if(c->phase == SrResyncIdle) {
        if(falling && in->session == SrSessionRunning) {
            c->phase = SrResyncWaitPeer;
            c->trigger_ms = in->now_ms;
            c->rx_at_trig = in->rx_bytes;
            c->rx_last = in->rx_bytes;
            c->rx_grow_ms = in->now_ms;
            c->rev_at_trig = in->session_rev;
            c->tries = 0;
            c->try_ms = 0;
            c->cmd_ms = 0;
            c->rev_at_step2 = 0;
        }
        /* Triggering while already Running must NOT count as "done" (the original
         * card hole). Stay in WaitPeer until rx grows and then goes quiet. */
        return SrResyncActNone;
    }

    if(c->phase == SrResyncLost) {
        if(sr_resync_recovered(c, in)) {
            sr_resync_go_idle(c);
        }
        return SrResyncActNone;
    }

    if(sr_resync_elapsed_ge(in->now_ms, c->trigger_ms, (uint32_t)SR_RESYNC_GIVEUP_MS)) {
        c->phase = SrResyncLost;
        return SrResyncActNone;
    }

    /* Operator finished stop+start by hand while we were about to send wardrive. */
    if((c->phase == SrResyncNeedStart || c->phase == SrResyncAwaitStart) &&
       sr_resync_recovered(c, in)) {
        sr_resync_go_idle(c);
        return SrResyncActNone;
    }

    /* Operator stopped scanning before we queued our own first stopscan.
     * After tries > 0 the model may already be Stopped (step 1 succeeded,
     * step 2 did not) — that is a retry, not a cancel. */
    if(c->phase == SrResyncWaitPeer && in->session != SrSessionRunning) {
        sr_resync_go_idle(c);
        return SrResyncActNone;
    }
    if(c->phase == SrResyncNeedStop && c->tries == 0u &&
       in->session != SrSessionRunning) {
        sr_resync_go_idle(c);
        return SrResyncActNone;
    }

    for(hops = 0; hops < 4u; hops++) {
        switch(c->phase) {
        case SrResyncWaitPeer:
            if((uint32_t)(in->rx_bytes - c->rx_last) > 0u) {
                c->rx_last = in->rx_bytes;
                c->rx_grow_ms = in->now_ms;
                return SrResyncActNone;
            }
            if((uint32_t)(c->rx_last - c->rx_at_trig) > 0u &&
               sr_resync_elapsed_ge(
                   in->now_ms, c->rx_grow_ms, (uint32_t)SR_RESYNC_RX_QUIET_MS)) {
                c->phase = SrResyncNeedStop;
                continue;
            }
            return SrResyncActNone;

        case SrResyncNeedStop:
            if(c->tries > 0u &&
               !sr_resync_elapsed_ge(
                   in->now_ms, c->try_ms, (uint32_t)SR_RESYNC_RETRY_GAP_MS)) {
                return SrResyncActNone;
            }
            return SrResyncActSendStop;

        case SrResyncAwaitStop:
            if((uint32_t)(in->session_rev - c->rev_at_trig) > 0u &&
               in->session == SrSessionStopped) {
                c->phase = SrResyncNeedStart;
                continue;
            }
            if(sr_resync_elapsed_ge(
                   in->now_ms, c->cmd_ms, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
                if(c->tries >= (uint8_t)SR_RESYNC_MAX_TRIES) {
                    c->phase = SrResyncLost;
                    return SrResyncActNone;
                }
                c->phase = SrResyncNeedStop;
                continue;
            }
            return SrResyncActNone;

        case SrResyncNeedStart:
            return SrResyncActSendStart;

        case SrResyncAwaitStart:
            /* C: do not time out into NeedStop. A late StartingWardrive must
             * still be able to finish; giveup (above) is the only abort. */
            if((uint32_t)(in->session_rev - c->rev_at_step2) > 0u &&
               in->session == SrSessionRunning) {
                sr_resync_go_idle(c);
                return SrResyncActNone;
            }
            return SrResyncActNone;

        default:
            return SrResyncActNone;
        }
    }
    return SrResyncActNone;
}
