#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "sr_types.h"

/* ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 * Per ADR-016 decision 4. The three DoD outcomes are exactly this function's return values.
 *
 * Auto-retry (one extra `info` after NoReply) is a separate predicate. It does
 * not change eval's 64-combo outcomes or the 1500 ms window. UnknownFw is not
 * retried: bytes already came back. */

enum { SR_HANDSHAKE_TIMEOUT_MS = 1500 };
enum { SR_HANDSHAKE_MAX_SENDS = 2 }; /* first queue + one auto retry */

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
    uint8_t sends; /* info queues this Probe visit; 0 = none */
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

/* True only for NoReply while sends is still below MAX. Scene queues one more
 * info and bumps sends; a failed queue must still bump sends so Tick cannot
 * spin. */
static inline bool sr_handshake_should_retry(const SrHandshakeCtx* c, uint32_t now_ms) {
    if(c == NULL || !c->sent) {
        return false;
    }
    if(c->sends >= (uint8_t)SR_HANDSHAKE_MAX_SENDS) {
        return false;
    }
    return sr_handshake_eval(c, now_ms) == SrHandshakeNoReply;
}
