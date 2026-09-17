#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sr_model.h" /* SrSessionState */

/*
 * ★ Pure-logic decision layer. Must not include any furi header (ADR-003).
 *
 * Card N6 -- seed ap_wifi / ap_ble / started_tick_ms from the board's Sess:
 * line after an N1 adoption.
 *
 * Seeding is a second, idempotent step, independent of dash_peer_sync_tick().
 * It must not ride firmware_rev: probe_n() returns true for each of
 * Firmware: / Version: / Hardware: / ESP-IDF: / Diag:, so one #info bumps
 * firmware_rev five times and the N1 one-shot path would disarm before Sess:
 * (the last line of that reply, n6-fap-sess-consume.md §2) has arrived.
 *
 * The predicate is "sess_rev changed since this Dash info was sent".
 * Snapshot sess_rev in on_enter **before** queuing info (sess_rev_at_send),
 * same shape as peer_sync_fw_rev. Do **not** snapshot at adopt: a 26 ms
 * #info often lands inside one 100 ms tick, so adopt would freeze the new
 * sess_rev and wait forever (n6-fap-sess-consume.md §5).
 *
 * sess_rev != 0 alone is "historically seen", which reuses a Probe-scene
 * snapshot and never corrects (C3/C4). Compared with != , never > : sess_rev
 * wraps. Pending is cleared only after a successful seed.
 */

typedef enum {
    SrSessSeedNone = 0, /* Leave the model alone */
    SrSessSeedApply,    /* Overlay sess.ap / sess.ble / sess.ms onto the live counters */
} SrSessSeedAct;

static inline SrSessSeedAct sr_sess_seed_eval(
    uint32_t sess_rev,
    uint32_t sess_rev_at_send,
    uint32_t sess_ms,
    SrSessionState cur,
    bool seed_pending) {
    if(!seed_pending) {
        /* Only an adopted session is seeded. A local SrEventScanStarted session
         * is counted by this FAP; overlaying the board's totals would clobber
         * the real counts. */
        return SrSessSeedNone;
    }
    if(sess_rev == 0u) {
        /* Contract ②: the line was never seen. Absence is unknown, not zero.
         * Same reason diag_seen exists (sr_types.h). */
        return SrSessSeedNone;
    }
    if(sess_rev == sess_rev_at_send) {
        /* No Sess: since this Dash queued info. The snapshot at send may be a
         * Probe-scene leftover; using it would paint stale AP/BLE/elapsed (C3).
         * This #info's Sess: (last line) will bump sess_rev. */
        return SrSessSeedNone;
    }
    if(sess_ms == 0u) {
        /* Contract ①: ms == 0 means no session is running, never "just started".
         * ap/ble on that line are the last session's totals and survive the seal.
         * This can disagree with Diag: st=1; on contradiction, refuse to seed. */
        return SrSessSeedNone;
    }
    if(cur != SrSessionRunning) {
        return SrSessSeedNone;
    }
    return SrSessSeedApply;
}
