#pragma once

#include "fsd_types.h" // CANFRAME (hardware-free); was ../libraries/mcp_can_2515.h
#include <stdbool.h>
#include <stdint.h>

#define CAN_ID_STW_ACTN_RQ    0x045 // 69 - steering wheel stalk (Legacy follow distance)
#define CAN_ID_AP_LEGACY      0x3EE // 1006 - autopilot control (Legacy)
#define CAN_ID_ISA_SPEED      0x399 // 921 - ISA speed chime (HW4)
#define CAN_ID_GTW_CAR_CONFIG 0x398 // 920 - HW version detection
#define CAN_ID_FOLLOW_DIST    0x3F8 // 1016 - follow distance / speed profile
#define CAN_ID_AP_CONTROL     0x3FD // 1021 - autopilot control (HW3/HW4)
#define CAN_ID_EPAS_STATUS    0x370 // 880 - EPAS3P_sysStatus (nag killer target)
#define CAN_ID_GTW_CAR_STATE  0x318 // 792 - GTW_carState (carries GTW_updateInProgress)
#define CAN_ID_BMS_HV_BUS     0x132 // 306 - BMS_hvBusStatus (pack voltage / current)
#define CAN_ID_BMS_SOC        0x292 // 658 - BMS_socStatus (state of charge)
#define CAN_ID_BMS_THERMAL    0x312 // 786 - BMS_thermalStatus (battery temp)
#define CAN_ID_TRIP_PLANNING  0x082 // 130 - UI_tripPlanning (precondition trigger)

// --- Extras CAN IDs (Model 3/Y) ---
#define CAN_ID_VCFRONT_LIGHT  0x3F5 // 1013 - ID3F5VCFRONT_lighting (hazard, fog, DRL, wiper)
#define CAN_ID_SCCM_RSTALK    0x229 // 553  - SCCM_rightStalk (gear shift, park button)
#define CAN_ID_DI_SYS_STATUS  0x118 // 280  - DI_systemStatus (track mode, traction ctrl)
#define CAN_ID_VCRIGHT_STATUS 0x343 // 835  - VCRIGHT_status (rear defrost state)
#define CAN_ID_DI_SPEED       0x257 // 599  - DI_speed (vehicle speed, checksummed)
#define CAN_ID_ESP_STATUS     0x145 // 325  - ESP_status (brake, stability)
#define CAN_ID_GTW_EPAS_CTRL  0x101 // 257  - GTW_epasControl (steering tune WRITE, Chassis CAN)
#define CAN_ID_DAS_STATUS \
    0x39B // 923  - DAS_status (HW4 + Highland HW3; AP state, nag, lane change, blind spot)
#define CAN_ID_DAS_STATUS_HW3 \
    0x399 // 921  - DAS_status (pre-Highland HW3 / Legacy, same ID as HW4 ISA chime — HW-dependent meaning)
#define CAN_ID_DAS_STATUS2   0x389 // 905  - DAS_status2 (ACC report, driver interaction)
#define CAN_ID_DAS_SETTINGS  0x293 // 659  - DAS_settings (autosteer enable, steering weight, etc.)
#define CAN_ID_DAS_AP_CONFIG 0x331 // 817  - DAS autopilot config (tier restore target, ~1 Hz)
#define CAN_ID_GTW_CONFIG_ETH \
    0x7FF // 2047 - GTW_carConfig on Ethernet/mixed bus (autopilot tier readback)
#define CAN_ID_TRACK_MODE_SET \
    0x313 // 787  - UI_trackModeSettings (track mode request, checksummed)
#define CAN_ID_SCCM_LSTALK \
    0x249 // 585  - SCCM_leftStalk (high beam, turn signal, wiper wash — Party CAN, 3 bytes)
#define CAN_ID_DI_TORQUE    0x108 // 264  - DI_torque (motor torque/power — Party CAN)
#define CAN_ID_DAS_CONTROL  0x2B9 // 697  - DAS_control (ACC state, set speed — Party CAN)
#define CAN_ID_DI_STATE     0x286 // 646  - DI_state (cruise state, gear, park brake — Party CAN)
#define CAN_ID_UI_WARNING   0x311 // 785  - UI_warning (blinker, door, buckle, wiper — Party CAN)
#define CAN_ID_ESP_WHEELSPD 0x175 // 373  - ESP_wheelSpeeds (4 wheel speeds — Party CAN)
#define CAN_ID_STEER_ANGLE  0x129 // 297  - SCCM_steeringAngleSensor (steering angle — Party CAN)
#define CAN_ID_DAS_STEER    0x488 // 1160 - DAS_steeringControl (DAS steering request — Party CAN)
#define CAN_ID_APS_EACMON   0x27D // 637  - APS_eacMonitor (steering permission — Party CAN)
#define CAN_ID_ENERGY_CONS  0x33A // 826  - UI_ratedConsumption (energy Wh/km — Party CAN)
#define CAN_ID_DRIVER_ASSIST \
    0x3F8 // 1016 - UI_driverAssistControl (also follow distance — Party CAN)
#define CAN_ID_VCLEFT_SWITCH \
    0x3C2 // 962  - VCLEFT_switchStatus (steering-wheel scrollwheel buttons — Vehicle CAN)

// TeslaHWVersion, OpMode, and FSDState are defined in the shared headers
// (fsd_types.h / fsd_state.h) so both the Flipper and ESP32 builds use one copy.
#include "fsd_state.h"

void fsd_state_init(FSDState* state, TeslaHWVersion hw);
void fsd_set_bit(CANFRAME* frame, int bit, bool value);
uint8_t fsd_read_mux_id(const CANFRAME* frame);
bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd);
TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame);

// AP-First stability debounce: AP must hold das_ap_state >= 2 for at least this
// long before injection is allowed, to avoid injecting on the activation edge
// (the steer-jerk window). Mirrors ev-open-can-tools v3.0.2-beta.2 (1 s).
#define AP_FIRST_STABLE_MS 1000u

/** AP-First gate. Returns true if injection is permitted right now: either
 *  AP-First is off, or AP is engaged (das_ap_state >= 2) and has been stable for
 *  >= AP_FIRST_STABLE_MS. now_ms is a millisecond clock; ap_unstable_tick_ms is
 *  stamped by the caller whenever das_ap_state < 2. */
bool fsd_ap_first_allows(const FSDState* state, uint32_t now_ms);

void fsd_handle_follow_distance(FSDState* state, const CANFRAME* frame);
bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame, uint32_t now_ms);

void fsd_handle_legacy_stalk(FSDState* state, const CANFRAME* frame);
bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame, uint32_t now_ms);
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
/** HW4 / Highland HW3 DAS_status parser (0x39B, party CAN layout). */
void fsd_handle_das_status_hw4(FSDState* state, const CANFRAME* frame);
/** Pre-Highland HW3 / Legacy DAS_status parser (0x399, legacy CAN layout).
 *  Same frame ID as HW4 ISA_SPEED — caller dispatches by HW version. */
void fsd_handle_das_status_hw3(FSDState* state, const CANFRAME* frame);

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

/** 0x7FF GTW Config Replay — snapshot learned-healthy state and replay
 *  any gateway-modified frames. Call on every 0x7FF frame. When not armed,
 *  captures the current frame as a "healthy" snapshot. When armed, compares
 *  incoming frame against snapshot and returns true if the frame was
 *  modified (caller should retransmit so the AP ECU sees the replayed
 *  version rather than the gateway's). Renamed from "Ban Shield" in v2.15
 *  to reflect actual behavior — broadcast-layer mask, not ban prevention. */
bool fsd_handle_gtw_shield(FSDState* state, CANFRAME* frame);

/** Modify 0x7FF mux=2 to force GTW_autopilot tier=SELF_DRIVING (3).
 *  More aggressive than GTW Config Replay — actively writes tier instead
 *  of replaying learned state. Returns true if frame was modified. */
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

/** Inject a human-like scroll-wheel AP engage gesture on 0x3C2 mux=1 — no 0x3FD
 *  touch required. Time-based state machine (press / scroll-up / press / scroll-up)
 *  per @JakNo's #82 design, fired on a das_ap_state UNAVAIL→AVAIL rising edge.
 *  `now_ms` is a millisecond clock (Flipper passes furi_get_tick(), 1 kHz).
 *  HW4 + Service mode only. Returns true if the frame was modified (caller
 *  should retransmit). Source: @JakNo in #43 / #82. */
bool fsd_handle_scroll_press_inject(FSDState* state, CANFRAME* frame, uint32_t now_ms);

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
