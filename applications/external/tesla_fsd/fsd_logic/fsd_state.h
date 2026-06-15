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
    uint8_t das_ap_state; // DAS_autopilotState: 0=UNAVAIL 1=AVAIL 2=ACTIVE_NOMINAL 3+=active
    uint32_t
        ap_unstable_tick_ms; // ms clock when das_ap_state was last < 2 (AP-first stability debounce)

    // --- Scroll-Press AP Engage (0x3C2 VCLEFT_switchStatus, HW4-only, Service mode) ---
    bool scroll_press_ap; // user toggle
    uint8_t
        scroll_press_state; // 0=idle/armed-track, 1=press1, 2=scroll1, 3=press2, 4=scroll-final, 5=cooldown
    bool scroll_press_armed; // true once das_ap_state==0 observed (required before each fire)
    uint32_t scroll_press_phase_ms; // now_ms at the start of the current phase (timing reference)

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
