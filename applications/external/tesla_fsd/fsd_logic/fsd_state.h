#pragma once
/*
 * fsd_state.h — the unified FSDState, shared by the Flipper and ESP32 builds.
 *
 * Previously each platform kept its own FSDState: the Flipper's was the richer
 * feature superset, the ESP32's carried firmware-specific fields (Wi-Fi,
 * display, OTA counters, NVS-backed flags). This is the union of both, so one
 * protocol core can compile for either side. Fields unused by a platform are
 * simply inert there. Behavior-preserving: no field changed type or meaning.
 */

#include "fsd_types.h" // TeslaHWVersion, OpMode, CANFRAME
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SpeedLimitSource_None = 0,
    SpeedLimitSource_Map,
    SpeedLimitSource_Vision,
    SpeedLimitSource_Acc,
} SpeedLimitSource;

typedef struct FSDState {
    TeslaHWVersion hw_version;
    int speed_profile;
    int speed_offset;
    bool fsd_enabled;
    bool nag_suppressed;
    uint32_t frames_modified;

    bool force_fsd;
    bool fsd_unlock; // enables core 0x3FD/0x3EE FSD activation TX
    bool suppress_speed_chime;
    bool emergency_vehicle_detect;
    bool nag_killer; // CAN 880 counter echo method
    bool nag_burst; // burst/pause echo (~1s on / ~1.5s off) — the rest
        // period is what lets a TSL6P-style device evade the
        // stricter 14.x detector (#122). Off by default.
    bool nag_epas_faithful; // v2.17 experimental: mirror the in-the-wild 0x370
        // scheme (no handsOnLevel flip; torque centred at
        // 0 Nm with live variance) to pass the 2026.14.x
        // content preflight. Default off — see #100.
    uint32_t nag_echo_count;
    bool nag_demand_active; // true while handsOnLevel == 0 or 3 — edge-detect source for on-demand grip pulse
    bool continuous_ap; // re-enable AP after AP drops while turn signal is active

    // operation mode + diagnostics
    OpMode op_mode;
    bool tesla_ota_in_progress; // pause TX while Tesla is updating
    uint32_t crc_err_count; // CAN bus error counter
    uint32_t rx_count; // total frames seen (for wiring sanity check)

    // live BMS data (read-only sniff)
    bool bms_seen;
    float pack_voltage_v;
    float pack_current_a;
    float soc_percent;
    int8_t batt_temp_min_c;
    int8_t batt_temp_max_c;

    // precondition trigger (writes 0x082 periodically)
    bool precondition;

    // --- extras: read-only vehicle state (parsed from bus) ---
    uint8_t track_mode_state; // 0=unavail 1=avail 2=on (from 0x118)
    uint8_t traction_ctrl_mode; // 0..7 (from 0x118)
    uint8_t rear_defrost_state; // 0=sna 1=on 2=off (from 0x343)
    float vehicle_speed_kph; // from 0x257 DI_vehicleSpeed (12-bit, 0.08 factor, -40 offset)
    uint8_t ui_speed; // from 0x257 DI_uiSpeed (8-bit, display value)
    uint8_t steering_tune_mode; // from 0x370 EPAS3S_currentTuneMode (0-6)
    float torsion_bar_torque_nm; // from 0x370 EPAS3S_torsionBarTorque
    bool torsion_bar_torque_seen;
    bool driver_brake_applied; // from 0x145 ESP_driverBrakeApply
    bool brake_status_seen; // true once a 0x145 ESP_status frame is parsed
        // (lets the continuous-AP brake interlock fail closed)
    bool speed_seen; // true once we've parsed at least one 0x257
    uint32_t last_speed_tick_ms; // ms clock when the last 0x257 was seen (TX interlock freshness)

    // --- AP-first mode (2026.14.x compatibility) ---
    bool ap_first; // delay 0x3FD/0x3EE injection until AP is engaged AND stable
    bool ap_first_edge; // experimental "Instant Engage": inject as soon as AP is
        // engaged, skipping the AP_FIRST_STABLE_MS debounce. Off by default.
    bool ap_first_minimal; // experimental "Minimal Inject": limit the AP-enable frame
        // injection to a brief burst at engage, then stop until the
        // car disengages — keeps injection off the abort edge (#108). Off by default.
    uint8_t
        ap_inject_count; // AP-enable frames modified this engagement (Minimal Inject burst budget;
        // reset to 0 on disengage, das_ap_state < DAS_APSTATE_ENGAGED)
    uint8_t das_ap_state; // DAS_autopilotState: 0=UNAVAIL 1=UNAVAILABLE/AVAIL-flicker
        // 2=AVAILABLE (offered, NOT engaged) 3=ACTIVE_NOMINAL (first
        // engaged) 6=active 8/9=aborting/aborted
    uint32_t
        ap_unstable_tick_ms; // ms clock when das_ap_state was last < DAS_APSTATE_ENGAGED (AP-first stability debounce)

    // --- Shared event-core bookkeeping (#123, fsd_events.h) ---
    // Per-instance state for fsd_events_poll() / fsd_events_inject(): detects
    // das_ap_state transitions and rate-limits each event type. No statics, so
    // the detector is host-testable and re-entrant.
    // FSD_EVENT_COUNT must match the FSDEventType enum in fsd_events.h, which
    // can't be included here without a cycle; fsd_events.h _Static_asserts it.
#define FSD_EVENT_COUNT 5
    uint8_t evt_prev_ap_state; // das_ap_state at the previous poll (transition source)
    uint32_t
        evt_cooldown_until_ms[FSD_EVENT_COUNT]; // per-type cooldown expiry, indexed by FSDEventType
    uint8_t evt_last_from; // last emitted event: from-state
    uint8_t evt_last_to; // last emitted event: to-state
    uint32_t evt_last_ms; // last emitted event: timestamp (now_ms)

    // --- Soft Engage (steer-jerk mitigation, #108) ---
    // Hold the activation-edge injection until the wheel is near-centred, so the
    // DAS's steering-path recompute at FSD-enable is small (the jerk is worse on
    // curves = bigger path delta). Latches once per engagement; resets when AP
    // drops. Needs 0x129 SCCM_steeringAngle on the tapped bus to be effective.
    bool soft_engage; // opt-in toggle
    bool soft_engage_latched; // true once activation has begun this engagement

    // --- Abort Guard (steer-jerk mitigation, #108) ---
    // On some cars the DAS engages then ABORTS within ~0.5s (DAS_autopilotState
    // 8/9), and that abort snaps the wheel — the steer-jerk. Root-caused from
    // dunckencn's logs: injected 0x3EE is byte-identical in jerk vs clean runs;
    // only the jerk runs reach the abort states. When on, cut all activation
    // injection the instant the car enters an abort state and latch off until a
    // clean disengage (das_ap_state < DAS_APSTATE_ENGAGED), so we never feed/repeat an in-progress
    // abort. Off by default; experimental, needs on-car validation.
    bool abort_guard; // opt-in toggle
    bool abort_guard_latched; // true once an abort was seen this engagement

    // --- Scroll-Press AP Engage (0x3C2 VCLEFT_switchStatus, HW4-only, Service mode) ---
    bool scroll_press_ap; // user toggle
    uint8_t
        scroll_press_state; // 0=idle/armed-track, 1=press1, 2=scroll1, 3=press2, 4=scroll-final, 5=cooldown
    bool scroll_press_armed; // true once das_ap_state==0 observed (required before each fire)
    uint32_t scroll_press_phase_ms; // now_ms at the start of the current phase (timing reference)

    // --- Configurable nag-context signal mapping (#122) ---
    // When cfg_das_id != 0, the RX path reads DAS_autopilotState + handsOnState
    // from these positions instead of the auto-detected parser — lets users with
    // non-standard 0x39B/0x399 layouts (per-car variants, e.g. byte0 vs byte1)
    // point the killer at the right bytes without per-car firmware fallbacks.
    uint16_t cfg_das_id; // DAS frame id (0 = auto-detect)
    uint8_t cfg_apstate_byte; // DAS_autopilotState position
    uint8_t cfg_apstate_shift;
    uint8_t cfg_apstate_mask;
    uint8_t cfg_handson_byte; // DAS_handsOnState position (same frame)
    uint8_t cfg_handson_shift;
    uint8_t cfg_handson_mask;
    uint16_t cfg_steer_id; // steering frame id (0 = auto, default 0x129)
    uint8_t cfg_steer_hi; // steering angle byte high / low (signed LE, *0.1)
    uint8_t cfg_steer_lo;
    uint32_t das_ctx_seen_ms; // last cfg-DAS update (freshness gate)
    uint32_t steer_ctx_seen_ms; // last cfg-steering update

    // --- DAS state (from 0x39B / 0x389 — Party CAN, read-only) ---
    uint8_t das_hands_on_state; // 0-15 (4-bit nag level from DAS, more precise than EPAS 2-bit)
    uint8_t das_prev_hands_on_state; // last das_hands_on_state, for the nag escalation edge (#100)
    uint8_t das_lane_change; // 0-31 (5-bit auto lane change state)
    uint8_t das_side_coll_warn; // 0-3  (side collision / blind spot warning)
    uint8_t das_side_coll_avoid; // 0-3  (side collision avoidance active)
    uint8_t das_fcw; // 0-3  (forward collision warning)
    uint8_t das_vision_speed_lim; // raw×5 = kph/mph
    uint8_t das_acc_report; // 0-24 (ACC state: 0=off, higher=active modes)
    uint8_t das_activation_fail; // 0-2  (why AP failed to activate)
    bool das_autosteer_on; // from 0x293 DAS_autosteerEnabled readback
    bool das_seen; // true once we've parsed any DAS_status hands-on source
    bool das_hw4_status_seen; // true once the HW4 0x39B DAS_status has been parsed
        // (gate for the 0x399 hands-on fallback on HW4 trims
        //  that never broadcast 0x39B, e.g. Juniper RWD on Bus 6)

    // --- HW4 0x39B byte0 AP-state auto-fallback (#116) ---
    // Highland (China MIC, fw 2026.20) ships an 8-byte HW4 0x39B but carries
    // DAS_autopilotState in byte0 low nibble (HW3 position) while byte1[7:4] is
    // pinned at 1 the whole drive. Detect that signature and latch this car to the
    // byte0 reading for the session (re-detected each power cycle via memset init).
    uint8_t das_hw4_byte0_pin_count; // consecutive (byte1[7:4]==1 && byte0>=2) frames
    bool das_hw4_byte1_moved; // sticky: byte1[7:4] ever seen != 1 (standard HW4)
    bool das_hw4_use_byte0; // one-way latch: read AP-state from byte0 low nibble

    // --- GTW autopilot tier (from 0x7FF mux=2 on mixed bus) ---
    int8_t gtw_autopilot_tier; // -1 = not yet read

    // --- 0x7FF GTW Config Replay (formerly "Ban Shield") ---
    uint8_t gtw_snapshot[8][8]; // [mux][byte0..7], 64 bytes total
    bool gtw_snapshot_valid[8]; // per-mux: has this mux been captured?
    bool gtw_shield_armed; // true = actively replaying changes
    uint32_t gtw_shield_blocks; // counter: how many frames we've replayed

    // --- upstream feature flags ---
    bool enhanced_autopilot; // when true, mux=1 also sets bit46 (EAP/summon)
    bool speed_profile_locked; // when true, follow distance won't override profile
    uint8_t hw4_offset; // HW4 mux=2 speed offset override (0 = no override)

    // --- DAS_control (0x2B9) — ACC / longitudinal state ---
    uint8_t das_acc_state; // 0-15 (0=cancel, 3=hold, 4=ACC_ON, 9=pause)
    float das_set_speed_kph; // set cruise speed (0.1 kph resolution)

    // --- DI_state (0x286) — cruise, gear, park brake ---
    uint8_t di_cruise_state; // 0-7 (0=unavail 1=standby 2=enabled 3=standstill)
    uint8_t di_park_brake_state; // 0-15
    uint8_t di_autopark_state; // 0-15
    uint8_t di_digital_speed; // 0.5 kph resolution (9-bit)

    // --- DI_torque (0x108) — motor power ---
    float di_torque_nm; // drive motor torque
    bool di_torque_seen;

    // --- UI_warning (0x311) — dashboard indicators ---
    bool ui_left_blinker;
    bool ui_right_blinker;
    bool ui_any_door_open;
    bool ui_buckle_status; // seatbelt
    bool ui_high_beam;
    bool ui_warning_seen;

    // --- steering angle (0x129) ---
    float steering_angle_deg;

    // --- DAS_steeringControl (0x488) ---
    float das_steer_angle_req; // DAS requested angle
    uint8_t das_steer_type; // 0=none 1=angle_ctrl 2=LKA 3=ELK

    // --- TLSSC Restore (0x331 DAS config spoof) ---
    bool tlssc_restore; // read-modify-retransmit 0x331 to set tier=SELF_DRIVING
    uint32_t tlssc_restore_count; // frames modified

    // --- 0x7FF active tier override (force SELF_DRIVING) ---
    bool gtw_tier_override; // actively write tier=3 on every 0x7FF mux=2

    // --- 0x3F8 driver assist overrides ---
    bool assist_nav_enable; // bit13 + bit48 + bit49: nav-based FSD routing
    bool assist_hands_off; // bit14: UI-level hands-on disable
    bool assist_dev_mode; // bit5: UI_dasDeveloper flag
    bool assist_lhd_override; // bit40-41: force left-hand drive

    // --- 0x3FD mux1 extras ---
    bool assist_show_lane_graph; // bit45: lane visualization on non-FSD tier
    bool assist_tlssc_bit38; // bit38 on mux0: explicit TLSSC enable (complementary to 0x331)

    // --- telemetry disable (0x3F8 bit43) ---
    bool assist_telemetry_off; // force UI_enableTripTelemetry=0

    // --- energy consumption (0x33A, read-only) ---
    float energy_wh_per_km;
    bool energy_seen;

    // --- extras: write toggles (BETA, Service mode only) ---
    bool extra_hazard_lights;
    bool extra_wiper_off;
    bool extra_park_inject; // inject a PARK stalk press
    uint8_t extra_steering_mode; // 0=no change, 1=comfort 2=standard 3=sport (GTW_epasTuneRequest)
    bool extra_highbeam_strobe; // rapid PULL/IDLE toggle on SCCM_leftStalk
    bool extra_turn_left; // inject left turn signal
    bool extra_turn_right; // inject right turn signal
    bool extra_wiper_wash; // inject wiper wash button press

    // ─────────────────────────────────────────────────────────────────────────
    // ESP32 firmware-only fields (inert on the Flipper build).
    // ─────────────────────────────────────────────────────────────────────────
    bool ap_active; // true when DAS reports AP/TACC active
    uint32_t tx_count; // total frames successfully transmitted

    bool ignore_ota; // allow TX while Tesla OTA is detected
    bool china_mode; // bypass FSD UI selection check for China vehicles

    // OTA detection debounce (from 0x318)
    uint8_t ota_raw_state; // raw GTW_updateInProgress bits [1:0]
    uint8_t ota_assert_count; // consecutive "in-progress" samples
    uint8_t ota_clear_count; // consecutive "not in-progress" samples

    // per-ID seen counters (wiring/diagnostics)
    uint32_t seen_gtw_car_state; // 0x318
    uint32_t seen_gtw_car_config; // 0x398
    uint32_t seen_ap_control; // 0x3FD
    uint32_t seen_bms_hv; // 0x132
    uint32_t seen_bms_soc; // 0x292
    uint32_t seen_bms_thermal; // 0x312

    bool bms_output; // print BMS data to serial
    uint32_t sleep_idle_ms; // CAN silence before deep sleep

    // Wi-Fi (ESP32 web dashboard)
    char wifi_ssid[33]; // max 32 chars + null
    char wifi_pass[65]; // max 64 chars + null
    char wifi_sta_ssid[33]; // optional station SSID
    char wifi_sta_pass[65]; // optional station password
    bool wifi_hidden;

    // 2026.14.x firmware warning (persisted in NVS on ESP32)
    bool firmware_14x_warning;

    // Black-box incident recorder (#124, persisted in NVS on ESP32). Default ON.
    // Auto-records full-rate CAN around aborts / bus-off / manual marks.
    bool blackbox_enabled;

    // DAS_status fields parsed by the ESP32 handler
    uint8_t das_speed_limit_1;
    uint8_t das_speed_limit_2;
    uint8_t das_lane_change_state;
    uint8_t das_counter;
    uint8_t das_checksum;

    // Continuous AP / read-only driver-assist state
    uint32_t stalk_full_up_ms;
    bool ap_ready;
    bool cruise_set_speed_seen;
    float cruise_set_speed_kph;
    bool speed_limit_seen;
    float speed_limit_kph;
    SpeedLimitSource speed_limit_source;
    uint32_t speed_limit_last_ms;
    bool left_turn_active;
    bool right_turn_active;
    bool left_turn_status_seen;
    bool right_turn_status_seen;
    bool turn_status_seen;
    float map_speed_limit_kph;
    float vision_speed_limit_kph;
    float acc_speed_limit_kph;

    // T-Display (ESP32 BOARD_TTGO_DISPLAY); kept unconditionally so the struct
    // layout is identical across boards.
    bool display_enabled;
    uint8_t display_brightness; // 0-100%
    uint32_t display_timeout_s;
} FSDState;
