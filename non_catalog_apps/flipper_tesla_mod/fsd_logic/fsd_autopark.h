#pragma once
/*
 * fsd_autopark.h — DAS_autopilotState engaged helper + in-car Autopark TX pause
 * (#180), shared by the Flipper and ESP32 builds.
 *
 * DAS_autopilotState (0x39B HW3/HW4, 0x399 pre-Highland/Legacy — byte0 low
 * nibble in both) value table:
 *   0 DISABLED  1 UNAVAILABLE  2 AVAILABLE  3 ACTIVE_NOMINAL  4 ACTIVE_RESTRICTED
 *   5 ACTIVE_NAV  6 ACTIVE_FSD (also the normal steady engaged state on newer
 *   firmware)  8 ABORTING  9 ABORTED  14 FAULT  15 SNA. Engaged = 3..6.
 *
 * In-car Autopark on Highland runs at DAS_autopilotState 6 with the autopark
 * bits set in byte3 (bit0 DAS_autoparkReady, bit1 DAS_autoParked, bit2
 * DAS_autoparkWaitingForBrake). Injecting during that window threw
 * AEB/traction/stability/regen warnings on the reporter's T-2CAN (#180). State 6
 * is ALSO real FSD engaged, so a blanket state-6 block would switch the nag
 * killer off for every HW4 FSD drive — instead we detect the Autopark *episode*
 * and pause all TX only while it runs.
 *
 * DAS_autoparkReady (bit0) is only "spot found / Autopark available" (the P
 * icon): it toggles during ordinary low-speed driving past parked cars, AP
 * engaged at 6 included, so on its own it must not open an episode — that
 * paused TX in city traffic every time the car dropped under 20 km/h and,
 * with the old rule, whenever AP re-engaged at a crawl near parked cars (#176).
 * A real in-car Autopark always enters 6 from a non-engaged state (parked, AP
 * not driving) with a maneuver bit already set (the #180 trace enters from
 * state 1 with byte3 0xE5 = waitingForBrake), so the maneuver bits — not
 * autoparkReady — are the Autopark-entry signal.
 *
 * Header-only static inline (the ESP32 build compiles no fsd_logic source
 * files), so fsd_logic/fsd_handler.c and esp32/.firmware/fsd_handler.cpp share
 * this one copy (host-tested by test/test_fsd_core.c and test_esp32_core.cpp).
 */

#include "fsd_state.h"
#include <stdbool.h>
#include <stdint.h>

// Engaged range: 3 ACTIVE_NOMINAL .. 6 ACTIVE_FSD. 8/9 abort, 14 fault, 15 SNA
// are NOT engaged.
#define FSD_DAS_ENGAGED_MIN 3u
#define FSD_DAS_ENGAGED_MAX 6u

// DAS_autopilotState value where in-car Autopark (and steady FSD) runs.
#define FSD_DAS_ACTIVE_FSD 6u

// A maneuver bit seen this recently still opens an episode as the state rises
// into 6 — the reporter's byte3 maneuver bits can lead the 1->6 jump by ~1 s.
#define FSD_AUTOPARK_BIT_RECENT_MS     3000u
// Vehicle speed must be at least this fresh to be trusted (entry + release checks).
#define FSD_AUTOPARK_SPEED_FRESH_MS    1000u
// Autopark starts from a standstill; a fresh valid reading over this at the
// entry into 6 means the car is already moving (AP/FSD engaging), not Autopark.
// kph (#176).
#define FSD_AUTOPARK_ENTRY_MAX_KPH     8.0f
// Autopark never runs above parking speed; a fresh reading over this ends a
// false episode for the rest of that state-6 period (#176). kph.
#define FSD_AUTOPARK_RELEASE_SPEED_KPH 20.0f
// DI_vehicleSpeed max valid raw is 4062 (= 284.96 kph); raw 4095 is SNA and
// decodes to 287.6 kph, which must NOT count as driving (it is unknown speed).
#define FSD_DI_SPEED_MAX_VALID_KPH     284.96f

// True for the engaged states (3..6), used wherever ap_active is derived.
static inline bool fsd_das_state_engaged(uint8_t s) {
    return s >= FSD_DAS_ENGAGED_MIN && s <= FSD_DAS_ENGAGED_MAX;
}

// Parse the three autopark bits from a DAS_status byte3 (same positions on
// 0x39B and 0x399) into FSDState. Called from both DAS parsers.
static inline void fsd_autopark_parse(FSDState* s, uint8_t byte3) {
    s->autopark_ready = (byte3 & 0x01u) != 0u;
    s->autopark_parked = (byte3 & 0x02u) != 0u;
    s->autopark_waiting_brake = (byte3 & 0x04u) != 0u;
}

// Maneuver bits only (DAS_autoParked / DAS_autoparkWaitingForBrake) — set by a
// running Autopark, unlike DAS_autoparkReady (spot found) (#176).
static inline bool fsd_autopark_maneuver_bit(const FSDState* s) {
    return s->autopark_parked || s->autopark_waiting_brake;
}

// Maintain the Autopark episode + TX-block state. Call once per RX frame from
// the RX loop, after the DAS status / vehicle-speed frames have been parsed
// (same vantage as fsd_abort_guard_update). now_ms is the loop's ms clock.
//
// Episode rule (#176):
//  - Entry (previous state != 6 -> 6): start an episode when the previous state
//    was <= 2 (Autopark enters 6 straight from 1, never from an engaged state)
//    OR a maneuver bit is set now OR a maneuver bit was set within
//    FSD_AUTOPARK_BIT_RECENT_MS — but NOT when fresh, valid speed is above
//    FSD_AUTOPARK_ENTRY_MAX_KPH (Autopark starts from a standstill). Unknown,
//    stale or SNA speed at entry still starts it (fail safe). autoparkReady
//    alone never opens an episode: entering 6 from an engaged state (prev >= 3)
//    with only that bit is AP re-engaging near parked cars, not Autopark.
//  - While in 6: only a maneuver bit starts/keeps an episode; autoparkReady
//    alone does not.
//  - Fresh, valid speed above FSD_AUTOPARK_RELEASE_SPEED_KPH ends the episode
//    for the rest of this state-6 period: only a maneuver bit can open a new one
//    before the state leaves 6 (slowing back down in traffic does not).
//  - Leaving state 6 ends the episode.
//
// Block = episode AND NOT (speed known, fresh, valid and clearly above parking
// speed). Unknown, stale or SNA speed keeps the block (fail safe).
static inline void fsd_autopark_update(FSDState* s, uint32_t now_ms) {
    uint8_t prev = s->autopark_prev_ap_state;
    uint8_t cur = s->das_ap_state;

    if(fsd_autopark_maneuver_bit(s)) s->autopark_maneuver_last_ms = now_ms;
    bool maneuver_recent = s->autopark_maneuver_last_ms != 0u &&
                           (uint32_t)(now_ms - s->autopark_maneuver_last_ms) <=
                               FSD_AUTOPARK_BIT_RECENT_MS;

    // SNA (raw 4095 = 287.6 kph) and above-max readings are unknown speed.
    bool speed_valid = s->speed_seen &&
                       (uint32_t)(now_ms - s->last_speed_tick_ms) <= FSD_AUTOPARK_SPEED_FRESH_MS &&
                       s->vehicle_speed_kph <= FSD_DI_SPEED_MAX_VALID_KPH;
    bool moving = speed_valid && s->vehicle_speed_kph > FSD_AUTOPARK_ENTRY_MAX_KPH;
    bool driving = speed_valid && s->vehicle_speed_kph > FSD_AUTOPARK_RELEASE_SPEED_KPH;

    if(cur == FSD_DAS_ACTIVE_FSD) {
        if(prev != FSD_DAS_ACTIVE_FSD && !moving &&
           (prev <= 2u || fsd_autopark_maneuver_bit(s) || maneuver_recent)) {
            s->autopark_episode = true;
        }
        if(fsd_autopark_maneuver_bit(s)) s->autopark_episode = true;
        // End, not just suppress: the entry rule only fires on the edge into 6,
        // so a released period stays released until a maneuver bit re-opens it.
        if(driving) s->autopark_episode = false;
    } else {
        s->autopark_episode = false;
    }

    s->autopark_tx_block = s->autopark_episode && !driving;

    s->autopark_prev_ap_state = cur;
}
