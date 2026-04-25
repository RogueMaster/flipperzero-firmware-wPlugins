#pragma once

#include "../libraries/mcp_can_2515.h"
#include <stdbool.h>
#include <stdint.h>

#define CAN_ID_STW_ACTN_RQ    0x045  // 69 - steering wheel stalk (Legacy follow distance)
#define CAN_ID_AP_LEGACY      0x3EE  // 1006 - autopilot control (Legacy)
#define CAN_ID_ISA_SPEED      0x399  // 921 - ISA speed chime (HW4)
#define CAN_ID_GTW_CAR_CONFIG 0x398  // 920 - HW version detection
#define CAN_ID_FOLLOW_DIST    0x3F8  // 1016 - follow distance / speed profile
#define CAN_ID_AP_CONTROL     0x3FD  // 1021 - autopilot control (HW3/HW4)
#define CAN_ID_EPAS_STATUS    0x370  // 880 - EPAS3P_sysStatus (nag killer target)
#define CAN_ID_GTW_CAR_STATE  0x318  // 792 - GTW_carState (carries GTW_updateInProgress)
#define CAN_ID_BMS_HV_BUS     0x132  // 306 - BMS_hvBusStatus (pack voltage / current)
#define CAN_ID_BMS_SOC        0x292  // 658 - BMS_socStatus (state of charge)
#define CAN_ID_BMS_THERMAL    0x312  // 786 - BMS_thermalStatus (battery temp)
#define CAN_ID_TRIP_PLANNING  0x082  // 130 - UI_tripPlanning (precondition trigger)

// --- Extras CAN IDs (Model 3/Y) ---
#define CAN_ID_VCFRONT_LIGHT  0x3F5  // 1013 - ID3F5VCFRONT_lighting (hazard, fog, DRL, wiper)
#define CAN_ID_SCCM_RSTALK   0x229  // 553  - SCCM_rightStalk (gear shift, park button)
#define CAN_ID_DI_SYS_STATUS  0x118  // 280  - DI_systemStatus (track mode, traction ctrl)
#define CAN_ID_VCRIGHT_STATUS 0x343  // 835  - VCRIGHT_status (rear defrost state)
#define CAN_ID_DI_SPEED       0x257  // 599  - DI_speed (vehicle speed, checksummed)
#define CAN_ID_ESP_STATUS     0x145  // 325  - ESP_status (brake, stability)
#define CAN_ID_GTW_EPAS_CTRL  0x101  // 257  - GTW_epasControl (steering tune WRITE, Chassis CAN)
#define CAN_ID_DAS_STATUS     0x39B  // 923  - DAS_status (AP state, nag, lane change, blind spot)
#define CAN_ID_DAS_STATUS2    0x389  // 905  - DAS_status2 (ACC report, driver interaction)
#define CAN_ID_DAS_SETTINGS   0x293  // 659  - DAS_settings (autosteer enable, steering weight, etc.)
#define CAN_ID_DAS_AP_CONFIG  0x331  // 817  - DAS autopilot config (tier restore target, ~1 Hz)
#define CAN_ID_GTW_CONFIG_ETH 0x7FF  // 2047 - GTW_carConfig on Ethernet/mixed bus (autopilot tier readback)
#define CAN_ID_TRACK_MODE_SET 0x313  // 787  - UI_trackModeSettings (track mode request, checksummed)
#define CAN_ID_SCCM_LSTALK   0x249  // 585  - SCCM_leftStalk (high beam, turn signal, wiper wash — Party CAN, 3 bytes)
#define CAN_ID_DI_TORQUE     0x108  // 264  - DI_torque (motor torque/power — Party CAN)
#define CAN_ID_DAS_CONTROL   0x2B9  // 697  - DAS_control (ACC state, set speed — Party CAN)
#define CAN_ID_DI_STATE      0x286  // 646  - DI_state (cruise state, gear, park brake — Party CAN)
#define CAN_ID_UI_WARNING    0x311  // 785  - UI_warning (blinker, door, buckle, wiper — Party CAN)
#define CAN_ID_ESP_WHEELSPD  0x175  // 373  - ESP_wheelSpeeds (4 wheel speeds — Party CAN)
#define CAN_ID_STEER_ANGLE   0x129  // 297  - SCCM_steeringAngleSensor (steering angle — Party CAN)
#define CAN_ID_DAS_STEER     0x488  // 1160 - DAS_steeringControl (DAS steering request — Party CAN)
#define CAN_ID_APS_EACMON    0x27D  // 637  - APS_eacMonitor (steering permission — Party CAN)
#define CAN_ID_ENERGY_CONS   0x33A  // 826  - UI_ratedConsumption (energy Wh/km — Party CAN)
#define CAN_ID_DRIVER_ASSIST 0x3F8  // 1016 - UI_driverAssistControl (also follow distance — Party CAN)

typedef enum {
    TeslaHW_Unknown = 0,
    TeslaHW_Legacy,
    TeslaHW_HW3,
    TeslaHW_HW4,
} TeslaHWVersion;

typedef enum {
    OpMode_Active = 0,    // RX + TX, normal operation
    OpMode_ListenOnly,    // pure passive sniff, no TX at all
    OpMode_Service,       // unrestricted, gates aggressive features
} OpMode;

typedef struct {
    TeslaHWVersion hw_version;
    int speed_profile;
    int speed_offset;
    bool fsd_enabled;
    bool nag_suppressed;
    uint32_t frames_modified;

    bool force_fsd;
    bool suppress_speed_chime;
    bool emergency_vehicle_detect;
    bool nag_killer;           // CAN 880 counter echo method
    uint32_t nag_echo_count;

    // operation mode + diagnostics
    OpMode op_mode;
    bool tesla_ota_in_progress;  // pause TX while Tesla is updating
    uint32_t crc_err_count;      // CAN bus error counter
    uint32_t rx_count;            // total frames seen (for wiring sanity check)

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
    uint8_t track_mode_state;    // 0=unavail 1=avail 2=on (from 0x118)
    uint8_t traction_ctrl_mode;  // 0..7 (from 0x118)
    uint8_t rear_defrost_state;  // 0=sna 1=on 2=off (from 0x343)
    float vehicle_speed_kph;     // from 0x257 DI_vehicleSpeed (12-bit, 0.08 factor, -40 offset)
    uint8_t ui_speed;            // from 0x257 DI_uiSpeed (8-bit, display value)
    uint8_t steering_tune_mode;  // from 0x370 EPAS3S_currentTuneMode (0-6)
    float torsion_bar_torque_nm; // from 0x370 EPAS3S_torsionBarTorque
    bool driver_brake_applied;   // from 0x145 ESP_driverBrakeApply
    bool speed_seen;             // true once we've parsed at least one 0x257

    // --- DAS state (from 0x39B / 0x389 — Party CAN, read-only) ---
    uint8_t das_hands_on_state;  // 0-15 (4-bit nag level from DAS, more precise than EPAS 2-bit)
    uint8_t das_lane_change;     // 0-31 (5-bit auto lane change state)
    uint8_t das_side_coll_warn;  // 0-3  (side collision / blind spot warning)
    uint8_t das_side_coll_avoid; // 0-3  (side collision avoidance active)
    uint8_t das_fcw;             // 0-3  (forward collision warning)
    uint8_t das_vision_speed_lim;// raw×5 = kph/mph
    uint8_t das_acc_report;      // 0-24 (ACC state: 0=off, higher=active modes)
    uint8_t das_activation_fail; // 0-2  (why AP failed to activate)
    bool das_autosteer_on;       // from 0x293 DAS_autosteerEnabled readback
    bool das_seen;               // true once we've parsed at least one 0x39B

    // --- GTW autopilot tier (from 0x7FF mux=2 on mixed bus) ---
    // 0=NONE 1=HIGHWAY 2=ENHANCED 3=SELF_DRIVING 4=BASIC
    // Source: ev-open-can-tools readGTWAutopilot()
    int8_t gtw_autopilot_tier;   // -1 = not yet read

    // --- 0x7FF shield (ban defense) ---
    // Snapshots of all 8 GTW_carConfig mux frames in "healthy" state.
    // When shield is armed: any incoming 0x7FF that differs from snapshot
    // is immediately retransmitted with the snapshot data, blocking
    // server-side ban pushes at the CAN layer.
    uint8_t gtw_snapshot[8][8];  // [mux][byte0..7], 64 bytes total
    bool gtw_snapshot_valid[8];  // per-mux: has this mux been captured?
    bool gtw_shield_armed;       // true = actively blocking changes
    uint32_t gtw_shield_blocks;  // counter: how many frames we've blocked

    // --- upstream feature flags ---
    bool enhanced_autopilot;     // when true, mux=1 also sets bit46 (EAP/summon)
    bool speed_profile_locked;   // when true, follow distance won't override profile
    uint8_t hw4_offset;          // HW4 mux=2 speed offset override (0 = no override)

    // --- DAS_control (0x2B9) — ACC / longitudinal state ---
    uint8_t das_acc_state;       // 0-15 (0=cancel, 3=hold, 4=ACC_ON, 9=pause)
    float das_set_speed_kph;     // set cruise speed (0.1 kph resolution)

    // --- DI_state (0x286) — cruise, gear, park brake ---
    uint8_t di_cruise_state;     // 0-7 (0=unavail 1=standby 2=enabled 3=standstill)
    uint8_t di_park_brake_state; // 0-15
    uint8_t di_autopark_state;   // 0-15
    uint8_t di_digital_speed;    // 0.5 kph resolution (9-bit)

    // --- DI_torque (0x108) — motor power ---
    float di_torque_nm;          // drive motor torque
    bool di_torque_seen;

    // --- UI_warning (0x311) — dashboard indicators ---
    bool ui_left_blinker;
    bool ui_right_blinker;
    bool ui_any_door_open;
    bool ui_buckle_status;       // seatbelt
    bool ui_high_beam;
    bool ui_warning_seen;

    // --- steering angle (0x129) ---
    float steering_angle_deg;

    // --- DAS_steeringControl (0x488) ---
    float das_steer_angle_req;   // DAS requested angle
    uint8_t das_steer_type;      // 0=none 1=angle_ctrl 2=LKA 3=ELK

    // --- TLSSC Restore (0x331 DAS config spoof) ---
    bool tlssc_restore;          // read-modify-retransmit 0x331 to set tier=SELF_DRIVING
    uint32_t tlssc_restore_count; // frames modified

    // --- 0x7FF active tier override (force SELF_DRIVING) ---
    bool gtw_tier_override;      // actively write tier=3 on every 0x7FF mux=2

    // --- 0x3F8 driver assist overrides (FUCKYOU-TESLA feature parity) ---
    bool assist_nav_enable;      // bit13 + bit48 + bit49: nav-based FSD routing
    bool assist_hands_off;       // bit14: UI-level hands-on disable
    bool assist_dev_mode;        // bit5: UI_dasDeveloper flag
    bool assist_lhd_override;    // bit40-41: force left-hand drive

    // --- 0x3FD mux1 extras ---
    bool assist_show_lane_graph; // bit45: lane visualization on non-FSD tier
    bool assist_tlssc_bit38;     // bit38 on mux0: explicit TLSSC enable (complementary to 0x331)

    // --- energy consumption (0x33A, read-only) ---
    float energy_wh_per_km;
    bool energy_seen;

    // --- extras: write toggles (BETA, Service mode only) ---
    bool extra_hazard_lights;
    bool extra_wiper_off;
    bool extra_park_inject;      // inject a PARK stalk press
    uint8_t extra_steering_mode; // 0=no change, 1=comfort 2=standard 3=sport (GTW_epasTuneRequest)
    bool extra_highbeam_strobe;   // rapid PULL/IDLE toggle on SCCM_leftStalk
    bool extra_turn_left;         // inject left turn signal
    bool extra_turn_right;        // inject right turn signal
    bool extra_wiper_wash;        // inject wiper wash button press
} FSDState;

void fsd_state_init(FSDState* state, TeslaHWVersion hw);
void fsd_set_bit(CANFRAME* frame, int bit, bool value);
uint8_t fsd_read_mux_id(const CANFRAME* frame);
bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd);
TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame);

void fsd_handle_follow_distance(FSDState* state, const CANFRAME* frame);
bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame);

void fsd_handle_legacy_stalk(FSDState* state, const CANFRAME* frame);
bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame);
bool fsd_handle_isa_speed_chime(CANFRAME* frame);

/** Handle CAN ID 0x370 - EPAS nag killer (counter+1 echo).
 *  Builds a new frame in out_frame. Returns true if should be sent. */
bool fsd_handle_nag_killer(FSDState* state, const CANFRAME* frame, CANFRAME* out_frame);

/** Handle CAN ID 0x318 - GTW_carState - update OTA-in-progress flag in state. */
void fsd_handle_gtw_car_state(FSDState* state, const CANFRAME* frame);

/** Returns true if the current state allows transmitting CAN frames. */
bool fsd_can_transmit(const FSDState* state);

/** Parse a BMS HV bus frame (0x132) and update voltage/current/power. */
void fsd_handle_bms_hv(FSDState* state, const CANFRAME* frame);

/** Parse a BMS SoC frame (0x292) and update soc_percent. */
void fsd_handle_bms_soc(FSDState* state, const CANFRAME* frame);

/** Parse a BMS thermal frame (0x312) and update battery temp min/max. */
void fsd_handle_bms_thermal(FSDState* state, const CANFRAME* frame);

/** Build a UI_tripPlanning frame (0x082) to trigger precondition heating. */
void fsd_build_precondition_frame(CANFRAME* frame);

// --- Extras: read-only parsers ---

/** Parse DI_systemStatus (0x118) — track mode state + traction control mode. */
void fsd_handle_di_system_status(FSDState* state, const CANFRAME* frame);

/** Parse VCRIGHT_status (0x343) — rear defrost state. */
void fsd_handle_vcright_status(FSDState* state, const CANFRAME* frame);

// --- Extras: write handlers (BETA, Service mode only) ---

/** Modify VCFRONT_lighting (0x3F5) to inject hazard light request.
 *  Sets VCFRONT_hazardLightRequest (byte0 bits 7:4) to HAZARD_REQUEST_BUTTON.
 *  Source: opendbc tesla_model3_vehicle.dbc line 235. */
bool fsd_handle_hazard_inject(const FSDState* state, CANFRAME* frame);

/** Modify DAS_bodyControls in 0x3F5 to set wiper speed to 0 (off).
 *  DAS_wiperSpeed (byte0 bits 7:4). Service mode only.
 *  Source: opendbc tesla_model3_vehicle.dbc line 199. */
bool fsd_handle_wiper_off(const FSDState* state, CANFRAME* frame);

/** Build a SCCM_rightStalk (0x229) frame simulating a PARK button press.
 *  SCCM_parkButtonStatus (byte2 bits 1:0) = 1 (PRESSED).
 *  Source: opendbc tesla_model3_vehicle.dbc line 126. */
void fsd_build_park_frame(CANFRAME* frame);

/** Parse DI_speed (0x257) — vehicle speed + UI speed.
 *  DI_vehicleSpeed: bit12|12, factor 0.08, offset -40, unit kph.
 *  DI_uiSpeed: bit24|8.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_di_speed(FSDState* state, const CANFRAME* frame);

/** Parse EPAS3S_currentTuneMode from the existing 0x370 frame.
 *  bit7|3 big-endian (0=fail_safe 1=comfort 2=standard 3=sport
 *  4=rwd_comfort 5=rwd_standard 6=rwd_sport).
 *  Also parses torsionBarTorque: bit19|12 big-endian, factor 0.01, offset -20.5.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_epas_steering_mode(FSDState* state, const CANFRAME* frame);

/** Parse ESP_status (0x145) — brake application state.
 *  ESP_driverBrakeApply: bit29|2.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_esp_status(FSDState* state, const CANFRAME* frame);

/** Build a GTW_epasControl (0x101) frame to set steering tune mode.
 *  GTW_epasTuneRequest: startBit 2, 3 bits (1=comfort 2=standard 3=sport).
 *  Source: tuncasoftbildik TESLA_CAN_STEERING_REFERENCE.md.
 *  NOTE: This is on CHASSIS CAN, not Party CAN — requires different tap. */
void fsd_build_steering_tune_frame(CANFRAME* frame, uint8_t mode);

/** Parse DAS_status (0x39B) — AP hands-on state, lane change, blind spot,
 *  FCW, vision speed limit. All Party CAN, read-only.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_das_status(FSDState* state, const CANFRAME* frame);

/** Parse DAS_status2 (0x389) — ACC report, activation failure.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_das_status2(FSDState* state, const CANFRAME* frame);

/** Parse DAS_settings (0x293) — readback of autosteer enabled state.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_handle_das_settings(FSDState* state, const CANFRAME* frame);

/** Parse GTW_carConfig (0x7FF) mux=2 — autopilot tier readback.
 *  byte[5] bits 4:2 → 0=NONE 1=HIGHWAY 2=ENHANCED 3=SELF_DRIVING 4=BASIC.
 *  Source: ev-open-can-tools readGTWAutopilot(). */
void fsd_handle_gtw_autopilot_tier(FSDState* state, const CANFRAME* frame);

/** 0x7FF shield — snapshot healthy state and block changes.
 *  Call on every 0x7FF frame. When shield is not armed, captures the
 *  current frame as the "healthy" snapshot. When armed, compares
 *  incoming frame against snapshot and returns true if the frame was
 *  modified (caller should retransmit the modified frame to override
 *  the Gateway's banned version). */
bool fsd_handle_gtw_shield(FSDState* state, CANFRAME* frame);

/** Modify 0x7FF mux=2 to force GTW_autopilot tier=SELF_DRIVING (3).
 *  More aggressive than shield — actively writes tier instead of freezing.
 *  Returns true if frame was modified. */
bool fsd_handle_gtw_tier_override(FSDState* state, CANFRAME* frame);

/** Modify 0x3F8 UI_driverAssistControl with region/nav/hands-off overrides.
 *  Bits: 5 (devMode), 13+48+49 (nav FSD), 14 (handsOff), 40-41 (drivingSide).
 *  Returns true if frame was modified (caller should retransmit). */
bool fsd_handle_driver_assist_override(FSDState* state, CANFRAME* frame);

/** Parse 0x33A UI_ratedConsumption — energy Wh/km. */
void fsd_handle_energy_consumption(FSDState* state, const CANFRAME* frame);

/** Modify UI_trackModeSettings (0x313) to set track mode ON.
 *  byte[0] bits 1:0 = 0x01 (kTrackModeRequestOn) + recalc checksum byte[7].
 *  Source: ev-open-can-tools setTrackModeRequest(). */
bool fsd_handle_track_mode_inject(FSDState* state, CANFRAME* frame);

/** Build a SCCM_leftStalk (0x249) frame for high beam strobe.
 *  SCCM_highBeamStalkStatus (bit12|2) = 1 (PULL) for flash.
 *  3-byte frame, CRC in byte0, counter in byte1[3:0].
 *  CRC = (0x49 + 0x02 + data[1] + data[2]) & 0xFF.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_build_highbeam_flash(CANFRAME* frame, uint8_t counter, bool flash_on);

/** Build a SCCM_leftStalk (0x249) frame for turn signal injection.
 *  SCCM_turnIndicatorStalkStatus (bit16|3): 1=UP_1(right), 3=DOWN_1(left).
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_build_turn_signal(CANFRAME* frame, uint8_t counter, uint8_t direction);

/** Handle CAN ID 0x331 — TLSSC Restore via DAS config spoof.
 *  Overwrites byte[0] lower 6 bits to 0x1B (DAS_autopilot=SELF_DRIVING,
 *  DAS_autopilotBase=SELF_DRIVING). Triggers MCU reboot and restores
 *  TLSSC toggle on banned vehicles.
 *  Returns true if frame was modified (caller should retransmit). */
bool fsd_handle_tlssc_restore(FSDState* state, CANFRAME* frame);

/** Build a SCCM_leftStalk (0x249) frame for wiper wash button press.
 *  SCCM_washWipeButtonStatus (bit14|2): 1=1ST_DETENT, 2=2ND_DETENT.
 *  Source: opendbc tesla_model3_party.dbc. */
void fsd_build_wiper_wash(CANFRAME* frame, uint8_t counter);

// --- Remaining Party CAN parsers ---

/** Parse DAS_control (0x2B9) — ACC state + set speed. */
void fsd_handle_das_control(FSDState* state, const CANFRAME* frame);

/** Parse DI_state (0x286) — cruise state, gear, park brake, digital speed. */
void fsd_handle_di_state(FSDState* state, const CANFRAME* frame);

/** Parse DI_torque (0x108) — motor torque. */
void fsd_handle_di_torque(FSDState* state, const CANFRAME* frame);

/** Parse UI_warning (0x311) — blinker, door, buckle, high beam status. */
void fsd_handle_ui_warning(FSDState* state, const CANFRAME* frame);

/** Parse SCCM_steeringAngleSensor (0x129) — steering wheel angle. */
void fsd_handle_steering_angle(FSDState* state, const CANFRAME* frame);

/** Parse DAS_steeringControl (0x488) — DAS steering request type + angle. */
void fsd_handle_das_steering(FSDState* state, const CANFRAME* frame);
