#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "sr_model.h" /* SrSessionState */

/* ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 * Per ADR-017 decision 1. The DoD item "start/stop mapping is correct" is equivalent to this
 * function reaching the right conclusion for every input. */

enum {
    SR_SCAN_CTL_TIMEOUT_MS = 2000
};
/* Dash identity bootstrap: one `info` while Version is empty. Same window as
 * Probe SR_HANDSHAKE_TIMEOUT_MS; scene_dash.c static-asserts the identity. */
enum {
    SR_SCAN_CTL_IDENT_MS = 1500
};

typedef enum {
    SrScanUiIdle = 0, /* Not scanning, no pending command */
    SrScanUiStarting, /* wardrive sent, awaiting StartingWardrive */
    SrScanUiRunning, /* Confirmed scanning */
    SrScanUiStopping, /* stopscan sent, awaiting Stopping... */
    SrScanUiStartFailed, /* start was sent and timed out unconfirmed */
    SrScanUiStopFailed, /* stop was sent and timed out unconfirmed */
    SrScanUiBusy, /* The command slot was occupied, so nothing was sent at all */
} SrScanUiState;

typedef struct {
    bool cmd_pending; /* A command was queued successfully and is awaiting confirmation */
    bool cmd_is_start; /* true=wardrive, false=stopscan */
    bool cmd_rejected; /* The last sr_worker_send_cmd returned false */
    uint32_t cmd_tick_ms; /* furi_get_tick() at the moment of queuing */
    uint32_t timeout_ms;
    uint32_t session_rev_at_send; /* Snapshot of model.session_rev taken at queue time */
    uint32_t session_rev_now; /* Refreshed on tick */
    SrSessionState session_now; /* Refreshed on tick */
} SrScanCtlCtx;

static inline SrScanUiState sr_scan_ctl_eval(const SrScanCtlCtx* c, uint32_t now_ms) {
    if(c == NULL) {
        return SrScanUiIdle;
    }
    if(c->cmd_pending) {
        const bool rev_rose = c->session_rev_now > c->session_rev_at_send;
        /* Unsigned subtraction: furi_get_tick overflows. Writing now > sent + timeout would jam
         * forever at the wraparound point. */
        const bool waiting = (uint32_t)(now_ms - c->cmd_tick_ms) < c->timeout_ms;
        if(c->cmd_is_start) {
            if(rev_rose && c->session_now == SrSessionRunning) return SrScanUiRunning;
            return waiting ? SrScanUiStarting : SrScanUiStartFailed;
        }
        if(rev_rose && c->session_now == SrSessionStopped) return SrScanUiIdle;
        return waiting ? SrScanUiStopping : SrScanUiStopFailed;
    }
    if(c->cmd_rejected) {
        return SrScanUiBusy;
    }
    return (c->session_now == SrSessionRunning) ? SrScanUiRunning : SrScanUiIdle;
}

typedef enum {
    SrScanActNone = 0, /* Do not queue a command */
    SrScanActSendStart, /* Queue wardrive */
    SrScanActSendStop, /* Queue stopscan */
} SrScanAct;

/* Dash OK-key mapping (D12 A1). Pure function so host_test can cover it. */
static inline SrScanAct sr_scan_ctl_on_ok(SrScanUiState st) {
    switch(st) {
    case SrScanUiRunning:
    case SrScanUiStarting:
    case SrScanUiStopFailed:
    case SrScanUiStopping:
        return SrScanActSendStop;
    case SrScanUiIdle:
    case SrScanUiStartFailed:
        return SrScanActSendStart;
    case SrScanUiBusy:
        return SrScanActNone;
    }
    return SrScanActNone;
}

/* Same window as SR_QUAL_STALE_MS (sr_view_fmt.h). Copied so this header
 * stays free of the view-fmt include. Idle Qual with sd==0 must still
 * block START (F2 §1C hides the health bar when sess_ms==0). */
enum {
    SR_SCAN_CTL_QUAL_STALE_MS = 15000
};

static inline bool
    sr_scan_ctl_sd_dead(uint32_t qual_rev, uint8_t sd, uint32_t qual_tick_ms, uint32_t now_ms) {
    if(qual_rev == 0u) {
        return false;
    }
    if((now_ms - qual_tick_ms) > (uint32_t)SR_SCAN_CTL_QUAL_STALE_MS) {
        return false;
    }
    return sd == 0u;
}

/* Diag: st=2 STOPPING / st=3 DRAINING — board is sealing. FAP session may
 * already be Stopped after the stopscan ack. */
static inline bool sr_scan_ctl_sealing(bool diag_seen, uint8_t diag_state) {
    return diag_seen && (diag_state == 2u || diag_state == 3u);
}

/* Live board 0=IDLE / 4=SEALED ends a stop-seal latch. Do not add
 * UPLOADING(5): that would clear the latch and allow START. */
static inline bool sr_scan_ctl_board_idle_or_sealed(uint8_t st) {
    return st == 0u || st == 4u;
}

/* Diag: st=5 UPLOADING (firmware session_gate.h). Independent of sealing. */
static inline bool sr_scan_ctl_uploading(bool diag_seen, uint8_t diag_state) {
    return diag_seen && diag_state == 5u;
}

static inline bool sr_scan_ctl_board_sealing_st(uint8_t st) {
    return st == 2u || st == 3u;
}

/* Latch after stopscan ack on SigRoam (or any peer that already sent Diag:).
 * Generic Marauder never emits Diag/Busy, so latching would block START forever. */
static inline bool sr_scan_ctl_should_latch_stop(bool is_sigroam, bool diag_seen) {
    return is_sigroam || diag_seen;
}

/*
 * Sealing for OK / Saving... :
 *   1. Fresh Diag or a Busy: received after this stop (busy_rev != at_stop)
 *      with st=0/4 → not sealing (seal finished, or IDLE refuse).
 *   2. Else Diag st=2/3 or that post-stop Busy st=2/3 → sealing.
 *   3. Else the local stop latch (stop-ack arrived, Diag still stale st=1).
 * Old Busy from a prior no-SD refuse is ignored: busy_rev_at_stop snapshots
 * busy_rev at stop confirm (0 if this app has never stopped).
 */
static inline bool sr_scan_ctl_sealing_ex(
    bool diag_seen,
    uint8_t diag_state,
    uint32_t busy_rev,
    uint32_t busy_rev_at_stop,
    uint8_t busy_state,
    bool stop_seal_latched) {
    const bool new_busy = busy_rev != busy_rev_at_stop;

    if(diag_seen && sr_scan_ctl_board_idle_or_sealed(diag_state)) {
        return false;
    }
    if(new_busy && sr_scan_ctl_board_idle_or_sealed(busy_state)) {
        return false;
    }
    if(sr_scan_ctl_sealing(diag_seen, diag_state)) {
        return true;
    }
    if(new_busy && sr_scan_ctl_board_sealing_st(busy_state)) {
        return true;
    }
    return stop_seal_latched;
}

/* Block START while Dash bootstrap is in flight and Version is still empty.
 * SR_SCAN_CTL_IDENT_MS is retry spacing in the scene, not the end of this
 * hold: expiring into "unknown = generic Marauder" was xu182. */
enum {
    SR_SCAN_CTL_IDENT_MAX_SENDS = 2
};

static inline bool sr_scan_ctl_ident_hold(
    bool pending,
    bool version_ready,
    uint32_t tick_ms,
    uint32_t now_ms,
    uint32_t timeout_ms) {
    (void)tick_ms;
    (void)now_ms;
    (void)timeout_ms;
    if(!pending || version_ready) {
        return false;
    }
    return true;
}

static inline bool sr_scan_ctl_ident_retry_due(
    bool pending,
    bool version_ready,
    uint8_t sends,
    uint32_t tick_ms,
    uint32_t now_ms,
    uint32_t timeout_ms) {
    if(!pending || version_ready) {
        return false;
    }
    if(sends >= (uint8_t)SR_SCAN_CTL_IDENT_MAX_SENDS) {
        return false;
    }
    return (uint32_t)(now_ms - tick_ms) >= timeout_ms;
}

/* Ident Link overlay must not hide State4 No SD / Saving. */
static inline bool
    sr_scan_ctl_ident_yields_state4(bool ident_overlay, bool sd_dead, bool sealing) {
    return ident_overlay && (sd_dead || sealing);
}

static inline bool sr_scan_ctl_allow_stop(SrScanUiState st) {
    return st == SrScanUiRunning || st == SrScanUiStarting || st == SrScanUiStopFailed ||
           st == SrScanUiStopping;
}

/* Busy means the stop never left. Session still Running: OK sends it. */
static inline SrScanAct sr_scan_ctl_retry_unconfirmed(
    SrScanAct act,
    SrScanUiState st,
    bool session_running,
    bool cmd_is_start) {
    if(act == SrScanActNone && st == SrScanUiBusy && session_running && !cmd_is_start) {
        return SrScanActSendStop;
    }
    return act;
}

static inline SrScanAct
    sr_scan_ctl_on_ok_ex(SrScanUiState st, bool sd_dead, bool sealing, bool ident_hold) {
    if(sd_dead || ident_hold || sealing) {
        return sr_scan_ctl_allow_stop(st) ? SrScanActSendStop : SrScanActNone;
    }
    return sr_scan_ctl_on_ok(st);
}

/* UPLOADING: neither wardrive nor stopscan. Do not fold into sealing
 * (sealing still allows Stop while FAP thinks Running). */
static inline SrScanAct sr_scan_ctl_on_ok_upload_gate(
    SrScanUiState st,
    bool sd_dead,
    bool sealing,
    bool ident_hold,
    bool uploading) {
    if(uploading) {
        return SrScanActNone;
    }
    return sr_scan_ctl_on_ok_ex(st, sd_dead, sealing, ident_hold);
}
