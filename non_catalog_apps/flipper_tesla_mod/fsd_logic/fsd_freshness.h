#pragma once
/*
 * fsd_freshness.h — transmit-freshness gate shared by both cores.
 *
 * A read-modify-retransmit path (nag echo, 0x3FD/0x3EE injection, the 0x25D /
 * 0x331 / 0x313 / 0x334 / 0x3F8 / 0x399 rewrites) builds its modified copy from
 * the frame it was handed and puts it on the bus at once. When the receive
 * backlog grows — the enlarged TWAI queue and the MCP2515 RX-task queue, or the
 * MCP2515's two hardware buffers reordering/delaying frames — a drain pass hands
 * those handlers frames up to a few hundred ms old, and the board emits a burst
 * of modified copies whose counters and payloads no longer match what the car is
 * sending now (#176). A retransmitted frame must track the car's live frame.
 *
 * The gate: a modified copy is transmitted only when its source frame is fresh.
 * Receive-side state (dashboard fields, DAS/EPAS state, black box, RX counts)
 * still updates for every frame — only the outgoing copy is suppressed.
 */

#include <stdint.h>
#include <stdbool.h>

// 0x370 EPAS3P_sysStatus is the fastest frame we echo: 100 Hz, a 10 ms period.
// A frame read within this window of its reception still tracks the car's live
// frame; one older is a backlogged copy. 25 ms = 2.5 frame periods: it passes a
// frame delayed by the normal loop jitter and the one driver/queue hop the
// current frame takes, while rejecting anything delayed by 3+ periods — the
// drain burst (frames 0.1–0.5 s old) that put stale copies on the bus. Slower
// frames (0x3FD ~50 Hz, 0x25D/0x331/0x313/0x334/0x3F8 well under that) are only
// more comfortably inside it, so one threshold covers every path.
#define FSD_FRESH_TX_MAX_AGE_MS 25u

// True when a frame received at rx_ms is fresh enough at now_ms that its
// modified copy may be transmitted. Unsigned subtraction handles the millis()
// wrap (rx_ms is always <= now_ms, both from the same clock). rx_ms == 0 means
// the producer did not stamp one (the Flipper driver, or a caller that cannot
// obtain a reliable timestamp): treat it as fresh so an unstamped path keeps its
// existing transmit behaviour rather than going silent.
static inline bool fsd_tx_fresh(uint32_t rx_ms, uint32_t now_ms) {
    if(rx_ms == 0u) return true;
    return (uint32_t)(now_ms - rx_ms) <= FSD_FRESH_TX_MAX_AGE_MS;
}

// Driver clearly has the wheel: |EPAS3P_torsionBarTorque| at or above this, in
// Nm. 2.5 Nm is above our own ±1.8 Nm nag-echo cap (NAG_TORQUE_RAW_*), so a real
// nag demand — which only arises when the car has seen ~0 torque — never reaches
// it and normal nagging is unaffected. (The reported handoff fight was -3.2 Nm.)
#define FSD_NAG_DRIVER_TORQUE_NM 2.5f

// True when the 0x370 nag echo must be suppressed because a spoofed hands-on
// would fight the driver or mask an AP handoff, rather than keep a live
// engagement alive (#176). Two cases, both gating the faithful and default echo
// paths:
//   - the car is aborting AP: das_ap_state 8 (ABORTING) or 9 (ABORTED). A nag
//     cannot save an engagement that is ending, and the spoof hides the handoff.
//     (das_ap_state reaches 8/9 only once a DAS status has been parsed, so an
//     unseen state of 0 never trips this.)
//   - the driver is applying real steering torque: overwriting their torque with
//     a fake low value fights them.
// torque_nm is the decoded EPAS3P_torsionBarTorque; both cores parse it (0x370
// steering parser) before the nag killer, so state->torsion_bar_torque_nm is this
// frame's value by the time this is called. Pure.
static inline bool fsd_nag_suppress_fault(uint8_t das_ap_state, float torque_nm) {
    if(das_ap_state == 8u || das_ap_state == 9u) return true;
    float a = torque_nm < 0.0f ? -torque_nm : torque_nm;
    return a >= FSD_NAG_DRIVER_TORQUE_NM;
}
