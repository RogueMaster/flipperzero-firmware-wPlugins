#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "sr_model.h" /* SrSessionState */

/* ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 * Per ADR-017 decision 1. The DoD item "start/stop mapping is correct" is equivalent to this
 * function reaching the right conclusion for every input. */

enum { SR_SCAN_CTL_TIMEOUT_MS = 2000 };

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
        return SrScanActSendStop;
    case SrScanUiIdle:
    case SrScanUiStartFailed:
        return SrScanActSendStart;
    case SrScanUiStopping:
    case SrScanUiBusy:
        return SrScanActNone;
    }
    return SrScanActNone;
}
