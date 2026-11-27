#pragma once
/*
 * fsd_rx_whitelist.h — derived RX ID whitelist for the opt-in "RX ID whitelist
 * (performance)" mode (default OFF).
 *
 * On a busy Tesla bus (~3300 frames/s) the firmware consumes only a few dozen
 * distinct CAN ids. When the whitelist mode is on, the ESP32 programs the CAN
 * controller's HARDWARE acceptance filter to just those ids, so an SPI-attached
 * controller (MCP2515 / MCP2518FD) isn't woken for every irrelevant frame.
 *
 * SAFETY — the whitelist is DERIVED here from current state, never a flat list a
 * maintainer has to remember to update. If an id a feature needs is ever dropped
 * the feature breaks silently, so the rule is: when in doubt, INCLUDE it. The set
 * is the union of:
 *   1. ESSENTIALS  — the HW-detect / OTA / DAS-status / nag / safety-interlock
 *      ids process_frame() acts on in EVERY mode, plus
 *   2. READ-OUTS   — the dashboard sniffs process_frame() parses UNCONDITIONALLY
 *      (BMS, TPMS, wheel speed, speed, map limit, …). The ESP32 firmware has no
 *      flag to disable a read-out, so these are always needed too.
 *   3. FEATURE IDS — ids a currently-enabled feature's handler needs that aren't
 *      already always-on (Track Mode 0x313, TLSSC 0x331).
 *   4. BLACK-BOX   — every FSD_BLACKBOX_KEY_IDS id while the recorder is on
 *      (adds 0x488 DAS_steeringControl, which process_frame() itself never reads).
 *   5. SIGNAL MAP  — the user-configured DAS / steering ids (#122) when set to a
 *      non-auto value.
 *
 * Accept-all (no hardware filter at all — today's behaviour) is forced whenever
 * the firmware must see arbitrary ids: a full / all-id capture stream, the
 * capability check, or a Signal-Map discovery scan; and whenever the derived set
 * would not fit the caller's buffer. Each controller ALSO independently falls
 * back to accept-all when the set exceeds ITS hardware filter capacity
 * (fits-or-accept-all, never a partial filter) — that decision lives in the
 * driver, not here.
 *
 * Header-only + dependency-light (fsd_state.h + fsd_blackbox_filter.h, no
 * Arduino) so the host test can exercise the derivation directly. Ids are given
 * as literals with their CAN_ID_* name in a comment, mirroring the config.h /
 * fsd_handler.h defines (same convention as fsd_blackbox_filter.h).
 */

#include "fsd_state.h"
#include "fsd_blackbox_filter.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Upper bound on the derived set the firmware will try to install. Larger than
// any single controller's filter capacity (MCP2515: 6, MCP2518FD: 32, TWAI:
// superset prefilter): the derivation returns accept-all if the union would
// exceed the caller's buffer, and each driver falls back to accept-all when the
// set exceeds its own hardware capacity.
#define FSD_RXW_MAX_IDS 40u

// ── 1. ESSENTIALS — acted on in EVERY mode regardless of any feature flag ─────
// HW detection / OTA pause / DAS-status gating / nag killer / safety interlocks.
// Dropping any of these breaks detection, the nag gate, or a safety auto-disable.
static const uint32_t FSD_RXW_ESSENTIAL_IDS[] = {
    0x398u, // CAN_ID_GTW_CAR_CONFIG  — HW-version detection
    0x318u, // CAN_ID_GTW_CAR_STATE   — OTA-in-progress (TX pause)
    0x399u, // CAN_ID_DAS_STATUS_HW3  — DAS status (HW3/Legacy) + HW4 handsOn fallback + ISA
    0x39Bu, // CAN_ID_DAS_STATUS_HW4  — DAS status (HW4) — AP state / nag gate
    0x370u, // CAN_ID_EPAS_STATUS     — nag killer target + EPAS read-out
    0x3EEu, // CAN_ID_AP_LEGACY       — Legacy autopilot + fallback HW-detect (0x3EE)
    0x3FDu, // CAN_ID_AP_CONTROL      — HW3/HW4 AP core + HW-detect fallback + Legacy->HW3 upgrade
    0x229u, // CAN_ID_SCCM_RSTALK     — Summon EU auto-disable (safety) + Continuous-AP gear lever
    0x257u, // CAN_ID_DI_SPEED        — Summon EU auto-disable (safety) + Autopark release gate
    0x145u, // CAN_ID_ESP_STATUS      — brake kill-switch (Continuous-AP interlock)
    0x118u, // CAN_ID_DI_SYS_STATUS   — DI_gear standstill gate + Track-Mode state read
    0x129u, // CAN_ID_STEER_ANGLE     — Soft Engage gate (when steer map is auto)
    0x045u, // CAN_ID_STW_ACTN_RQ     — Legacy follow-distance stalk (Legacy only)
};
#define FSD_RXW_ESSENTIAL_COUNT (sizeof(FSD_RXW_ESSENTIAL_IDS) / sizeof(FSD_RXW_ESSENTIAL_IDS[0]))

// ── 2. READ-OUTS — parsed UNCONDITIONALLY by process_frame() for the dashboard ─
// There is no runtime flag to turn a read-out off, so every one is always needed:
// filtering it out silently freezes that dashboard value.
static const uint32_t FSD_RXW_READOUT_IDS[] = {
    0x132u, // CAN_ID_BMS_HV_BUS      — pack voltage / current
    0x292u, // CAN_ID_BMS_SOC         — state of charge
    0x312u, // CAN_ID_BMS_THERMAL     — battery temperature
    0x33Au, // CAN_ID_UI_RANGE        — car-displayed SOC
    0x219u, // CAN_ID_VCSEC_TPMS      — tyre pressures
    0x175u, // CAN_ID_ESP_WHEELSPD    — wheel speeds
    0x238u, // CAN_ID_UI_MAP_DATA     — map speed limit
    0x389u, // CAN_ID_DAS_STATUS2     — ACC speed limit / report
    0x2B9u, // CAN_ID_DAS_CONTROL     — ACC state / set speed
    0x3F5u, // CAN_ID_VCFRONT_LIGHT   — turn-signal request state
    0x25Du, // CAN_ID_CP_STATUS       — charge-port type (read-only decode)
    0x3F8u, // CAN_ID_FOLLOW_DIST     — follow distance / speed profile
    0x334u, // CAN_ID_UI_POWERTRAIN   — pedal map (car's own, read-only)
};
#define FSD_RXW_READOUT_COUNT (sizeof(FSD_RXW_READOUT_IDS) / sizeof(FSD_RXW_READOUT_IDS[0]))

// ── 3. FEATURE IDS — needed ONLY while a specific feature flag is on ──────────
// (Other feature handlers — nag killer, FSD/AP, accel mode, CP rewrite, follow
//  assist — read ids already covered by groups 1/2, so enabling them widens the
//  set by nothing. The precondition feature only TRANSMITS 0x082, never receives
//  it, so it adds no RX id.)
static const uint32_t FSD_RXW_TRACK_MODE_IDS[] = {
    0x313u, // CAN_ID_TRACK_MODE_SET  — Track Mode inject (read-modify-retransmit)
};
static const uint32_t FSD_RXW_TLSSC_IDS[] = {
    0x331u, // CAN_ID_DAS_AP_CONFIG   — TLSSC Restore (read-modify-retransmit)
};

// Append `id` (masked to 11 bits) to out[0..*n) unless already present or full.
// Returns false and leaves the set unchanged when it is already at `cap`
// (overflow — the caller turns this into accept-all).
static inline bool fsd_rxw_add(uint32_t* out, size_t cap, size_t* n, uint32_t id) {
    id &= 0x7FFu;
    for(size_t i = 0; i < *n; i++) {
        if(out[i] == id) return true; // already in the set
    }
    if(*n >= cap) return false; // would overflow the caller's buffer
    out[*n] = id;
    (*n)++;
    return true;
}

static inline bool
    fsd_rxw_add_table(uint32_t* out, size_t cap, size_t* n, const uint32_t* ids, size_t count) {
    for(size_t i = 0; i < count; i++) {
        if(!fsd_rxw_add(out, cap, n, ids[i])) return false;
    }
    return true;
}

/*
 * Build the derived RX whitelist for the current state.
 *
 *   s                          — current FSDState (feature flags, Signal Map ids).
 *   blackbox_on                — the black-box recorder is enabled.
 *   capture_all_active         — a full / all-id HTTP-CAN capture stream is live
 *                                (no ?ids= filter) — the firmware must see every id.
 *   capability_active          — the tap capability check is counting (#125).
 *   signalmap_discovery_active — a Signal-Map discovery scan wants arbitrary ids.
 *   out_ids / cap              — caller buffer and its size.
 *   out_n                      — filled with the derived count (0 on accept-all).
 *
 * Returns true  => ACCEPT-ALL: install no hardware filter (today's behaviour),
 *                  because a fallback condition holds or the union exceeds `cap`.
 * Returns false => a derived set of *out_n ids was written to out_ids; the caller
 *                  installs it on controllers that can hold an exact superset,
 *                  and accept-all on the rest.
 */
static inline bool fsd_rx_whitelist_needed(
    const FSDState* s,
    bool blackbox_on,
    bool capture_all_active,
    bool capability_active,
    bool signalmap_discovery_active,
    uint32_t* out_ids,
    size_t cap,
    size_t* out_n) {
    if(out_n) *out_n = 0;
    if(!out_ids || cap == 0) return true; // nowhere to build — accept-all

    // Fallback: any mode where the firmware needs arbitrary ids.
    if(capture_all_active || capability_active || signalmap_discovery_active) {
        return true;
    }

    size_t n = 0;
    bool ok = true;

    // 1. Essentials + 2. read-outs — always.
    ok = ok && fsd_rxw_add_table(out_ids, cap, &n, FSD_RXW_ESSENTIAL_IDS, FSD_RXW_ESSENTIAL_COUNT);
    ok = ok && fsd_rxw_add_table(out_ids, cap, &n, FSD_RXW_READOUT_IDS, FSD_RXW_READOUT_COUNT);

    // 3. Feature-gated ids.
    if(s && s->track_mode_inject) {
        ok = ok && fsd_rxw_add_table(
                       out_ids,
                       cap,
                       &n,
                       FSD_RXW_TRACK_MODE_IDS,
                       sizeof(FSD_RXW_TRACK_MODE_IDS) / sizeof(uint32_t));
    }
    if(s && s->tlssc_restore) {
        ok =
            ok &&
            fsd_rxw_add_table(
                out_ids, cap, &n, FSD_RXW_TLSSC_IDS, sizeof(FSD_RXW_TLSSC_IDS) / sizeof(uint32_t));
    }

    // 4. Black-box key ids.
    if(blackbox_on) {
        ok = ok &&
             fsd_rxw_add_table(out_ids, cap, &n, FSD_BLACKBOX_KEY_IDS, FSD_BLACKBOX_KEY_ID_COUNT);
    }

    // 5. Signal-Map configured ids (non-auto).
    if(s && s->cfg_das_id != 0) ok = ok && fsd_rxw_add(out_ids, cap, &n, s->cfg_das_id);
    if(s && s->cfg_steer_id != 0) ok = ok && fsd_rxw_add(out_ids, cap, &n, s->cfg_steer_id);

    if(!ok) { // the union overran the buffer — safest is accept-all
        if(out_n) *out_n = 0;
        return true;
    }
    if(out_n) *out_n = n;
    return false;
}
