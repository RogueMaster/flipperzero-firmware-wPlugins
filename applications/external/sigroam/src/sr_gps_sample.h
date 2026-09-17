#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sr_model.h" /* SrSessionState */
#include "sr_scan_ctl.h" /* SrScanUiState */
#include "sr_view_fmt.h" /* SR_VIEW_COLS */

/* ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 * Per ADR-020: sample one GPS frame on demand while idle; the guards ensure
 * nothing is sent during a scan, and every sample is closed out with a stop. */

enum {
    SR_GPS_FIRST_BLOCK_MS = 8000, /* Soft timeout: changes wording only; sends no command, enters no terminal state */
    /* Hard timeout: give up and send stopscan (D3, decided by the user 2026-08-20).
     * Basis for 40000: cold start measured at ~30 s, extrapolated from a single
     * data point plus 33% headroom -- not a multi-sample statistic.
     * The risk is covered by "Back can leave at any time during the wait, and
     * on_exit sends the trailing stopscan". */
    SR_GPS_SLOW_BLOCK_MS = 40000,
    SR_GPS_STOP_ACK_MS = 2000, /* Close-out confirmation window, same magnitude as SR_SCAN_CTL_TIMEOUT_MS */
};

typedef enum {
    SrGpsPhaseIdle = 0,   /* No sample in flight */
    SrGpsPhaseWaitBlock,  /* gpsdata sent, awaiting the first block */
    SrGpsPhaseWaitStop,   /* stopscan sent, awaiting confirmation */
    SrGpsPhaseDone,       /* Block received and close-out confirmed */
    SrGpsPhaseNoReply,    /* First block timed out (no GPS module, or no response) */
    SrGpsPhaseStopUnsure, /* Block received but close-out unconfirmed -- the next wardrive may be silently ignored */
    SrGpsPhaseRejected,   /* The command never got queued; nothing was sent */
    SrGpsPhaseWaitSlow,   /* = 7: past the soft timeout, still awaiting the first block (GPS may be cold starting) */
} SrGpsPhase;

typedef enum {
    SrGpsGateOk = 0,
    SrGpsGateNoLink,   /* Serial port not open, or the codec does not support it */
    SrGpsGateScanning, /* A wardrive is running -- sending would silently kill the capture (V-060 (1)) */
    SrGpsGateBusy,     /* A start/stop command is in flight */
    SrGpsGateInFlight, /* A sample is already in flight */
} SrGpsGate;

typedef enum { SrGpsActNone = 0, SrGpsActSendStop } SrGpsAct;

typedef struct {
    uint8_t phase;             /* SrGpsPhase */
    bool got_block;            /* Whether this round has received a block */
    uint32_t sent_tick_ms;     /* Instant gpsdata was sent */
    uint32_t stop_tick_ms;     /* Instant stopscan was sent */
    uint32_t blocks_at_send;   /* Snapshot of model.gps_blocks when gpsdata was sent */
    uint32_t stop_rev_at_send; /* Snapshot of model.gps_stop_rev when stopscan was sent */
} SrGpsSampleCtx;

typedef struct {
    uint8_t phase;  /* New phase */
    uint8_t act;    /* SrGpsAct */
    bool got_block; /* New got_block */
} SrGpsStep;

/*
 * Evaluation order is priority order and must not be reordered (safety checks
 * must win over advisory ones).
 * The third check uses scan_ui rather than scan.cmd_pending: the latter is only
 * cleared while the Drive scene is alive, so leaving StartFailed via Back would
 * pin it at true forever and silently lock sampling out.
 */
static inline SrGpsGate sr_gps_gate(bool link_ok, uint8_t session, uint8_t scan_ui, uint8_t phase) {
    if(!link_ok) {
        return SrGpsGateNoLink;
    }
    if(session == (uint8_t)SrSessionRunning) {
        return SrGpsGateScanning;
    }
    if(scan_ui == (uint8_t)SrScanUiStarting || scan_ui == (uint8_t)SrScanUiRunning ||
       scan_ui == (uint8_t)SrScanUiStopping) {
        return SrGpsGateBusy;
    }
    if(phase == (uint8_t)SrGpsPhaseWaitBlock || phase == (uint8_t)SrGpsPhaseWaitSlow ||
       phase == (uint8_t)SrGpsPhaseWaitStop) {
        return SrGpsGateInFlight;
    }
    return SrGpsGateOk;
}

/* Pure function: does not modify c, only returns the next step. Time comparisons
 * always use unsigned subtraction; counter comparisons use !=. */
static inline SrGpsStep sr_gps_step(
    const SrGpsSampleCtx* c, uint32_t blocks_now, uint32_t stop_rev_now, uint32_t now_ms) {
    SrGpsStep s;

    s.phase = c->phase;
    s.act = (uint8_t)SrGpsActNone;
    s.got_block = c->got_block;

    if(c->phase == (uint8_t)SrGpsPhaseWaitBlock) {
        if(blocks_now != c->blocks_at_send) {
            s.phase = (uint8_t)SrGpsPhaseWaitStop;
            s.act = (uint8_t)SrGpsActSendStop;
            s.got_block = true;
        } else if((uint32_t)(now_ms - c->sent_tick_ms) >= (uint32_t)SR_GPS_FIRST_BLOCK_MS) {
            s.phase = (uint8_t)SrGpsPhaseWaitSlow;
            s.act = (uint8_t)SrGpsActNone;
            s.got_block = false;
        }
        return s;
    }
    if(c->phase == (uint8_t)SrGpsPhaseWaitSlow) {
        if(blocks_now != c->blocks_at_send) {
            s.phase = (uint8_t)SrGpsPhaseWaitStop;
            s.act = (uint8_t)SrGpsActSendStop;
            s.got_block = true;
        } else if((uint32_t)(now_ms - c->sent_tick_ms) >= (uint32_t)SR_GPS_SLOW_BLOCK_MS) {
            s.phase = (uint8_t)SrGpsPhaseWaitStop;
            s.act = (uint8_t)SrGpsActSendStop;
            s.got_block = false;
        }
        return s;
    }
    if(c->phase == (uint8_t)SrGpsPhaseWaitStop) {
        if(stop_rev_now != c->stop_rev_at_send) {
            s.phase = c->got_block ? (uint8_t)SrGpsPhaseDone : (uint8_t)SrGpsPhaseNoReply;
        } else if((uint32_t)(now_ms - c->stop_tick_ms) >= (uint32_t)SR_GPS_STOP_ACK_MS) {
            s.phase = c->got_block ? (uint8_t)SrGpsPhaseStopUnsure : (uint8_t)SrGpsPhaseNoReply;
        }
        return s;
    }
    /* Invariant: phase == Idle <=> SrGpsSampleCtx is all zeros. This branch only
     * returns Idle; the scene layer must memset the whole ctx and must never
     * assign phase alone. */
    if(c->phase == (uint8_t)SrGpsPhaseNoReply) {
        /* Terminal-state review (D4): NoReply is "the conclusion of this sampling
         * round", not "a durable assertion about the world".
         * Actually receiving a block is irrefutable contrary evidence -> this
         * round's context is void, so return to Idle.
         * The counter comparison must be != (it wraps); written as > , all 384
         * exhaustive cases stay green and only the wraparound assertion epsilon
         * catches it.
         * Invariant: phase == Idle <=> ctx all zeros, so got_block is cleared to
         * false here as well, with the scene layer's memset covering the rest. */
        if(blocks_now != c->blocks_at_send) {
            s.phase = (uint8_t)SrGpsPhaseIdle;
            s.act = (uint8_t)SrGpsActNone;
            s.got_block = false;
        }
        return s;
    }
    return s;
}

/* Returns a static literal or NULL, always <= SR_VIEW_COLS(20). idle_hint affects Idle only. */
static inline const char* sr_gps_status_text(uint8_t phase, uint8_t gate, bool idle_hint) {
    if(phase == (uint8_t)SrGpsPhaseWaitBlock || phase == (uint8_t)SrGpsPhaseWaitStop) {
        return "Sampling GPS...";
    }
    if(phase == (uint8_t)SrGpsPhaseWaitSlow) {
        return "Waiting for GPS...";
    }
    if(phase == (uint8_t)SrGpsPhaseDone) {
        return NULL;
    }
    if(phase == (uint8_t)SrGpsPhaseNoReply) {
        return "No GPS reply";
    }
    if(phase == (uint8_t)SrGpsPhaseStopUnsure) {
        return "Stop unconfirmed";
    }
    if(phase == (uint8_t)SrGpsPhaseRejected) {
        return "Radio busy, retry";
    }
    if(phase != (uint8_t)SrGpsPhaseIdle) {
        return NULL;
    }
    if(!idle_hint) {
        return NULL;
    }
    if(gate == (uint8_t)SrGpsGateOk) {
        return "Press OK to sample";
    }
    if(gate == (uint8_t)SrGpsGateScanning) {
        return "Wardrive running";
    }
    if(gate == (uint8_t)SrGpsGateBusy) {
        return "Radio busy";
    }
    return NULL;
}
