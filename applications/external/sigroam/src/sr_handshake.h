#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "sr_types.h"

/* ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 * Per ADR-016 decision 4. The three DoD outcomes are exactly this function's return values. */

enum { SR_HANDSHAKE_TIMEOUT_MS = 1500 };

typedef enum {
    SrHandshakeIdle = 0,  /* Probe not yet sent */
    SrHandshakeWaiting,   /* Sent, awaiting a reply */
    SrHandshakeOk,        /* Marauder recognized */
    SrHandshakeUnknownFw, /* Bytes came back, but not Marauder */
    SrHandshakeNoReply,   /* Timed out with not a single byte returned */
} SrHandshakeState;

typedef struct {
    bool sent;
    uint32_t sent_tick_ms;
    uint32_t rx_bytes_at_send;
    uint32_t rx_bytes_now;
    uint32_t fw_rev_at_send; /* Snapshot of model.firmware_rev taken in on_enter */
    uint32_t fw_rev_now;     /* Refreshed on tick */
    SrSourceKind fw_kind;
    uint32_t timeout_ms;
} SrHandshakeCtx;

static inline SrHandshakeState sr_handshake_eval(const SrHandshakeCtx* c, uint32_t now_ms) {
    if(c == NULL || !c->sent) {
        return SrHandshakeIdle;
    }
    if(c->fw_rev_now > c->fw_rev_at_send && c->fw_kind == SrSourceMarauder) {
        return SrHandshakeOk;
    }
    /* Unsigned subtraction: furi_get_tick overflows. Writing now > sent + timeout would jam in
     * Waiting forever at the wraparound point. */
    if((uint32_t)(now_ms - c->sent_tick_ms) < c->timeout_ms) {
        return SrHandshakeWaiting;
    }
    if(c->rx_bytes_now > c->rx_bytes_at_send) {
        return SrHandshakeUnknownFw;
    }
    return SrHandshakeNoReply;
}
