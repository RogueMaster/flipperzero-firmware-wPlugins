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

// An autopark bit seen this recently still opens an episode as the state rises
// into 6 — the reporter's DAS_autoparkReady 0->1 led the 1->6 jump by ~1.1 s.
#define FSD_AUTOPARK_BIT_RECENT_MS     3000u
// Vehicle speed must be at least this fresh to be trusted for the release check.
#define FSD_AUTOPARK_SPEED_FRESH_MS    1000u
// Autopark never runs above parking speed; a fresh reading over this releases a
// false episode as soon as the car is really driving. kph.
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

static inline bool fsd_autopark_any_bit(const FSDState* s) {
    return s->autopark_ready || s->autopark_parked || s->autopark_waiting_brake;
}

// Maintain the Autopark episode + TX-block state. Call once per RX frame from
// the RX loop, after the DAS status / vehicle-speed frames have been parsed
// (same vantage as fsd_abort_guard_update). now_ms is the loop's ms clock.
//
// Episode rule: on entering state 6 (previous state != 6) start an episode when
// the previous state was <= 2 (Autopark enters 6 straight from 1; FSD reaches 6
// through 3) OR an autopark bit is set now OR one was set within the last
// FSD_AUTOPARK_BIT_RECENT_MS. While in state 6, an autopark bit going set also
// starts/keeps the episode. Leaving state 6 ends it.
//
// Block = episode AND NOT (speed known, fresh, valid and clearly above parking
// speed). Unknown, stale or SNA speed keeps the block (fail safe).
static inline void fsd_autopark_update(FSDState* s, uint32_t now_ms) {
    uint8_t prev = s->autopark_prev_ap_state;
    uint8_t cur = s->das_ap_state;

    if(fsd_autopark_any_bit(s)) s->autopark_bit_last_ms = now_ms;
    bool bit_recent = s->autopark_bit_last_ms != 0u &&
                      (uint32_t)(now_ms - s->autopark_bit_last_ms) <= FSD_AUTOPARK_BIT_RECENT_MS;

    if(cur == FSD_DAS_ACTIVE_FSD) {
        if(prev != FSD_DAS_ACTIVE_FSD && (prev <= 2u || fsd_autopark_any_bit(s) || bit_recent)) {
            s->autopark_episode = true;
        }
        if(fsd_autopark_any_bit(s)) s->autopark_episode = true;
    } else {
        s->autopark_episode = false;
    }

    bool speed_fresh = s->speed_seen &&
                       (uint32_t)(now_ms - s->last_speed_tick_ms) <= FSD_AUTOPARK_SPEED_FRESH_MS;
    bool driving = speed_fresh && s->vehicle_speed_kph > FSD_AUTOPARK_RELEASE_SPEED_KPH &&
                   s->vehicle_speed_kph <= FSD_DI_SPEED_MAX_VALID_KPH;
    s->autopark_tx_block = s->autopark_episode && !driving;

    s->autopark_prev_ap_state = cur;
}
