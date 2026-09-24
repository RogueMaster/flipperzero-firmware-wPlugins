#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sr_model.h"  /* SrSessionState */
#include "sr_resync.h" /* SR_RESYNC_HINT_* */

/*
 * ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 *
 * R1 (docs/exec-plans/r1-peer-reset-resync.md): keep "Dash says Running" true
 * on the SigRoam dialect. The model only knows what it sent; the board's own
 * Diag: st and Sess: ms in each #info reply say whether it is still scanning.
 * Measured 2026-09-24 on the lab Scout Lite:
 *   - USB plug-in reset the ESP (sigroam-0001 stops at 17:59:10, sigroam-0002
 *     uptime puts the boot at 17:59:15). Legacy resync only watches the VBUS
 *     falling edge, so the model stayed Running with frozen counters.
 *   - USB unplug did not reset it (sigroam-0392 uptime continuous), yet legacy
 *     resync sent stopscan, and its wardrive hit seal + auto-upload (~34 s +
 *     ~28 s) and got Busy: forever.
 * So on SigRoam the edge-driven legacy resync is off and this reconcile owns
 * recovery; generic Marauder keeps sr_resync.h unchanged.
 *
 * GRACE 15000 = SR_QUAL_STALE_MS, product choice: covers START accepted but
 * the board not yet SCANNING. BAD 2 consecutive fresh replies (10 s at the
 * 5 s Dash refresh), product choice. RETRY 5000 = SR_RESYNC_RETRY_GAP_MS.
 * GIVEUP 180000 is a product choice sized over one measured seal + upload
 * (62 s); it is not a measurement.
 */

enum {
    SR_PEER_WATCH_GRACE_MS = 15000,
    SR_PEER_WATCH_BAD_REPLIES = 2,
    SR_PEER_WATCH_RETRY_GAP_MS = 5000,
    SR_PEER_WATCH_GIVEUP_MS = 180000
};

_Static_assert(SR_PEER_WATCH_BAD_REPLIES >= 2, "one bad reply must never restart a session");
_Static_assert(
    SR_PEER_WATCH_GIVEUP_MS > 62000, "giveup must outlast one measured seal + upload (62 s)");

/* Diag: st=1 is SCANNING (firmware session_gate.h:21, sr_peer_sync.h SrPeerStScanning). */
enum { SR_PEER_WATCH_ST_SCANNING = 1 };

typedef enum {
    SrPeerWatchIdle = 0,
    SrPeerWatchRecovering, /* model marked Stopped; wardrive until the board accepts */
    SrPeerWatchLost        /* gave up; hint stays until a session starts again */
} SrPeerWatchPhase;

typedef enum {
    SrPeerWatchActNone = 0,
    SrPeerWatchActRestart,   /* mark the model Stopped, then queue wardrive */
    SrPeerWatchActSendStart, /* queue wardrive again */
} SrPeerWatchAct;

typedef struct {
    bool sigroam;
    SrSessionState session;
    uint32_t session_rev;
    bool cmd_pending;
    uint32_t sess_rev; /* bumps once per Sess: line, the last line of each #info reply */
    uint32_t sess_ms;
    bool diag_seen;
    uint8_t diag_state;
    uint32_t now_ms;
} SrPeerWatchIn;

typedef struct {
    SrPeerWatchPhase phase;
    bool primed;
    uint32_t seen_session_rev;
    uint32_t rev_change_ms;
    uint32_t seen_sess_rev;
    uint8_t bad;
    uint32_t trigger_ms;
    uint32_t try_ms;
    bool sent;
    uint32_t rev_at_send;
} SrPeerWatchCtx;

static inline bool sr_peer_watch_legacy_resync_allowed(bool sigroam) {
    return !sigroam;
}

static inline void sr_peer_watch_init(SrPeerWatchCtx* c) {
    if(c == NULL) {
        return;
    }
    memset(c, 0, sizeof(*c));
}

static inline bool sr_peer_watch_elapsed_ge(uint32_t now_ms, uint32_t then_ms, uint32_t dur_ms) {
    return (uint32_t)(now_ms - then_ms) >= dur_ms;
}

static inline void sr_peer_watch_rearm(SrPeerWatchCtx* c, const SrPeerWatchIn* in) {
    c->phase = SrPeerWatchIdle;
    c->primed = true;
    c->seen_session_rev = in->session_rev;
    c->rev_change_ms = in->now_ms;
    c->seen_sess_rev = in->sess_rev;
    c->bad = 0;
    c->sent = false;
}

static inline void
    sr_peer_watch_note_sent(SrPeerWatchCtx* c, uint32_t now_ms, uint32_t session_rev_at_send) {
    if(c == NULL) {
        return;
    }
    c->sent = true;
    c->try_ms = now_ms;
    c->rev_at_send = session_rev_at_send;
}

static inline uint8_t sr_peer_watch_hint(const SrPeerWatchCtx* c) {
    if(c == NULL || c->phase == SrPeerWatchIdle) {
        return (uint8_t)SR_RESYNC_HINT_NONE;
    }
    if(c->phase == SrPeerWatchLost) {
        return (uint8_t)SR_RESYNC_HINT_LOST;
    }
    return (uint8_t)SR_RESYNC_HINT_BUSY;
}

static inline SrPeerWatchAct sr_peer_watch_eval(SrPeerWatchCtx* c, const SrPeerWatchIn* in) {
    bool not_scanning;

    if(c == NULL || in == NULL) {
        return SrPeerWatchActNone;
    }
    if(!in->sigroam) {
        sr_peer_watch_init(c);
        return SrPeerWatchActNone;
    }

    if(c->phase != SrPeerWatchIdle) {
        /* A new session started (ours or the operator's OK): watch it afresh. */
        if(in->session == SrSessionRunning && in->session_rev != c->rev_at_send) {
            sr_peer_watch_rearm(c, in);
            return SrPeerWatchActNone;
        }
        if(c->phase == SrPeerWatchLost) {
            return SrPeerWatchActNone;
        }
        if(sr_peer_watch_elapsed_ge(in->now_ms, c->trigger_ms, (uint32_t)SR_PEER_WATCH_GIVEUP_MS)) {
            c->phase = SrPeerWatchLost;
            return SrPeerWatchActNone;
        }
        if(!c->sent ||
           sr_peer_watch_elapsed_ge(in->now_ms, c->try_ms, (uint32_t)SR_PEER_WATCH_RETRY_GAP_MS)) {
            return SrPeerWatchActSendStart;
        }
        return SrPeerWatchActNone;
    }

    if(!c->primed || in->session_rev != c->seen_session_rev) {
        sr_peer_watch_rearm(c, in);
        return SrPeerWatchActNone;
    }
    if(in->session != SrSessionRunning || in->cmd_pending ||
       !sr_peer_watch_elapsed_ge(in->now_ms, c->rev_change_ms, (uint32_t)SR_PEER_WATCH_GRACE_MS)) {
        c->seen_sess_rev = in->sess_rev;
        c->bad = 0;
        return SrPeerWatchActNone;
    }
    if(in->sess_rev == c->seen_sess_rev) {
        return SrPeerWatchActNone;
    }
    c->seen_sess_rev = in->sess_rev;

    not_scanning = in->sess_ms == 0u ||
                   (in->diag_seen && in->diag_state != (uint8_t)SR_PEER_WATCH_ST_SCANNING);
    if(!not_scanning) {
        c->bad = 0;
        return SrPeerWatchActNone;
    }
    if(c->bad < 255u) {
        c->bad++;
    }
    if(c->bad < (uint8_t)SR_PEER_WATCH_BAD_REPLIES) {
        return SrPeerWatchActNone;
    }
    c->phase = SrPeerWatchRecovering;
    c->trigger_ms = in->now_ms;
    c->try_ms = in->now_ms;
    c->sent = false;
    c->rev_at_send = in->session_rev;
    c->bad = 0;
    return SrPeerWatchActRestart;
}
