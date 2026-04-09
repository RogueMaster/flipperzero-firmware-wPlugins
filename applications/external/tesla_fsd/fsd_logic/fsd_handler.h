#pragma once

#include "../libraries/mcp_can_2515.h"
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

typedef enum {
    TeslaHW_Unknown = 0,
    TeslaHW_Legacy,
    TeslaHW_HW3,
    TeslaHW_HW4,
} TeslaHWVersion;

typedef enum {
    OpMode_Active = 0, // RX + TX, normal operation
    OpMode_ListenOnly, // pure passive sniff, no TX at all
    OpMode_Service, // unrestricted, gates aggressive features
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
    bool nag_killer; // CAN 880 counter echo method
    uint32_t nag_echo_count;

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
    uint8_t track_mode_state; // 0=unavail 1=avail 2=on (from 0x118 DI_trackModeState)
    uint8_t traction_ctrl_mode; // 0..5 (from 0x118 DI_tractionControlMode)
    uint8_t rear_defrost_state; // 0=sna 1=on 2=off (from 0x343 VCRIGHT_rearDefrostState)

    // --- extras: write toggles (BETA, Service mode only) ---
    bool extra_hazard_lights;
    bool extra_wiper_off;
    bool extra_park_inject; // inject a PARK stalk press
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
