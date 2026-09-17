#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sr_model.h" /* SrSessionState */

/*
 * ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 *
 * Card N1 -- adopt the board's session on app launch.
 *
 * This model starts at SrSessionIdle every time the app launches, while the board
 * keeps scanning after the app exits. That asymmetry is deliberate, not a bug:
 * scene_start.c:13-16 states it outright -- "while a scan runs, Back returns to the
 * main menu without stopping the scan (Stop is triggered only by OK on the Dash
 * tab)" -- because the product use is "the board drives itself, the Flipper is only
 * a remote". Nothing on the exit path sends stopscan; the only four build_stop_cmd
 * call sites are resync, the two GPS-sample close-outs, and the Dash OK key.
 *
 * So after a relaunch the two sides disagree, and the Dash OK key maps to wardrive
 * (sr_scan_ctl.h:53 falls through to SrScanUiIdle, and sr_scan_ctl_on_ok maps Idle
 * to SrScanActSendStart), which the board refuses. Measured 2026-09-09 over the
 * Flipper USB-UART bridge, board left SCANNING by an earlier FAP session:
 *
 *     TX  info      -> Diag: st=1 seal=00  /  CmdTrace: boot=1 post=1/0 deq=1 accept=1 open=1/0
 *     TX  wardrive  -> #wardrive
 *                      Busy: st=1 seal=00
 *     TX  info      -> CmdTrace unchanged, byte for byte; state still SCANNING
 *
 * Refusing is correct on the board's side -- firmware session_gate.h:28
 * sr_session_start_acceptable() accepts only IDLE or SEALED, so a new session cannot
 * clobber one that has not finished sealing. The fix therefore belongs here: believe
 * the Diag: line instead of assuming Idle. Decided by the user 2026-09-09; changing
 * the firmware and auto-stopping on exit were both ruled out.
 */

/*
 * Peer session state as reported by the Diag: line. These are the firmware's
 * sr_session_state_t values verbatim; that enum carries the comment "Numeric values
 * are ADR-19 and must not change" (firmware main/tasks/session_gate.h:19-26), and
 * scenes/scene_probe.c:63-64 already renders them in this order.
 */
typedef enum {
    SrPeerStIdle = 0,
    SrPeerStScanning = 1,
    SrPeerStStopping = 2,
    SrPeerStDraining = 3,
    SrPeerStSealed = 4,
    SrPeerStUploading = 5,
} SrPeerState;

/* The wire depends on these three numbers, so pin them: a renumbering here would
 * silently start adopting the wrong state rather than failing anywhere visible. */
_Static_assert(SrPeerStIdle == 0, "Diag: st=0 is IDLE (firmware session_gate.h:20)");
_Static_assert(SrPeerStScanning == 1, "Diag: st=1 is SCANNING (firmware session_gate.h:21)");
_Static_assert(SrPeerStSealed == 4, "Diag: st=4 is SEALED (firmware session_gate.h:24)");

typedef enum {
    SrPeerSyncNone = 0, /* Leave the model alone; disarm the one-shot */
    SrPeerSyncAdoptRunning, /* The board is mid-session: adopt it so OK maps to stopscan */
    SrPeerSyncWait, /* Diag: of this #info has not arrived; keep pending */
} SrPeerSyncAct;

/*
 * diag_seen / diag_state come from SrFirmwareInfo, filled by the Diag: parser
 * (sr_parse_marauder.c:438, strict -- a half-read line is rejected outright).
 * cur is this model's session; cmd_pending is app->scan.cmd_pending.
 *
 * Only SCANNING is adopted. The other three refusing states are left alone on
 * purpose, and this is the whole of the reasoning, so it does not have to be
 * re-derived later:
 *   - STOPPING (2) / DRAINING (3): the board is already sealing, which takes seconds,
 *     and it answers Busy: st=2 / st=3 on its own. sr_scan_ctl.h reacts to that
 *     through busy_rev (dash_scan_tick clears cmd_pending on an explicit refusal), so
 *     the UI already says something true. Adopting them would need a fourth UI state
 *     for a window measured in seconds.
 *   - UPLOADING (5): reserved for firmware T5.3, and session_gate.h:17-19 says T2.5
 *     must never enter it. Adopting a state the board cannot currently report would be
 *     untestable code.
 * ⚠ So this function does NOT make every Busy go away -- it removes the one that
 * lasts as long as the session does. Scope decided by the user 2026-09-09.
 *
 * cmd_pending is a hard guard, not a nicety. A stopscan queued by the OK key leaves
 * cmd_pending true while the board still reports SCANNING for a few hundred ms; a
 * Diag: reply landing in that window would re-adopt Running and bump session_rev,
 * and session_rev rising is exactly the confirmation signal sr_scan_ctl_eval watches
 * (rev_rose). Adopting there would corrupt the in-flight command's verdict.
 */
static inline SrPeerSyncAct sr_peer_sync_eval(
    bool diag_seen,
    uint8_t diag_state,
    SrSessionState cur,
    bool cmd_pending) {
    if(!diag_seen) {
        /* Older firmware, a truncated line, or this #info's Firmware: has
         * already invalidated the previous Diag: (sr_parse_marauder.c probe_n).
         * Wait, do not disarm: the first firmware_rev bump is Firmware:, not
         * Diag:. Returning None here used to one-shot the pending flag and
         * never adopt (C1). An all-zero SrFirmwareInfo would otherwise read
         * as a confident "IDLE" -- which is why diag_seen exists
         * (sr_types.h:143-144). */
        return SrPeerSyncWait;
    }
    if(cmd_pending) {
        return SrPeerSyncNone;
    }
    if(diag_state != (uint8_t)SrPeerStScanning) {
        return SrPeerSyncNone;
    }
    if(cur == SrSessionRunning) {
        /* Already agreed. Re-adopting would reset the statistics and bump session_rev
         * on every info reply -- the Dash sends one on entry, so this branch is hit
         * on the second entry within one launch, not some rare race. */
        return SrPeerSyncNone;
    }
    return SrPeerSyncAdoptRunning;
}

/*
 * GUI-tick policy for the N1 one-shot. scene_dash.c must not clear
 * peer_sync_pending except by writing keep_pending from this function.
 *
 * keep_pending stays true when no reply has raised firmware_rev yet, and
 * when eval returns Wait (this #info's Diag: not seen). Every other act
 * disarms — including AdoptRunning, not-SCANNING, already-Running, and
 * cmd_pending.
 */
typedef struct {
    bool keep_pending;
    SrPeerSyncAct act;
} SrPeerSyncTick;

static inline SrPeerSyncTick sr_peer_sync_on_tick(
    bool pending,
    uint32_t fw_rev,
    uint32_t snap,
    bool diag_seen,
    uint8_t diag_state,
    SrSessionState cur,
    bool cmd_pending) {
    SrPeerSyncTick t;

    t.keep_pending = false;
    t.act = SrPeerSyncNone;
    if(!pending) {
        return t;
    }
    /* != , not > : firmware_rev wraps, same as cmdack, busy_rev and the furi tick. */
    if(fw_rev == snap) {
        t.keep_pending = true;
        return t;
    }
    t.act = sr_peer_sync_eval(diag_seen, diag_state, cur, cmd_pending);
    if(t.act == SrPeerSyncWait) {
        t.keep_pending = true;
        return t;
    }
    return t;
}
