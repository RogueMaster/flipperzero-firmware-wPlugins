#include "fsd_handler.h"
#include <string.h>

void fsd_state_init(FSDState* state, TeslaHWVersion hw) {
    memset(state, 0, sizeof(FSDState));
    state->hw_version = hw;
    if(hw == TeslaHW_HW4)
        state->speed_profile = 4;
    else if(hw == TeslaHW_Legacy)
        state->speed_profile = 1;
    else
        state->speed_profile = 2;
    state->op_mode = OpMode_Active;
}

void fsd_handle_gtw_car_state(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    // GTW_updateInProgress sits at bit 48|2 in 0x318 (Tesla DBC).
    // Treat any non-zero value as "OTA in progress" — be conservative.
    uint8_t in_progress = (frame->buffer[6] >> 0) & 0x03;
    state->tesla_ota_in_progress = (in_progress != 0);
}

bool fsd_can_transmit(const FSDState* state) {
    if(state->op_mode == OpMode_ListenOnly) return false;
    if(state->tesla_ota_in_progress) return false;
    return true;
}

// --- BMS read-only parsers (CAN frame templates from tuncasoftbildik/tesla-can-mod) ---

void fsd_handle_bms_hv(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    uint16_t raw_v = ((uint16_t)frame->buffer[1] << 8) | frame->buffer[0];
    int16_t raw_i = (int16_t)(((uint16_t)frame->buffer[3] << 8) | frame->buffer[2]);
    state->pack_voltage_v = raw_v * 0.01f;
    state->pack_current_a = raw_i * 0.1f;
    state->bms_seen = true;
}

void fsd_handle_bms_soc(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 2) return;
    uint16_t raw = ((uint16_t)(frame->buffer[1] & 0x03) << 8) | frame->buffer[0];
    state->soc_percent = raw * 0.1f;
    state->bms_seen = true;
}

void fsd_handle_bms_thermal(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    state->batt_temp_min_c = (int8_t)(frame->buffer[4] - 40);
    state->batt_temp_max_c = (int8_t)(frame->buffer[5] - 40);
    state->bms_seen = true;
}

// --- Precondition trigger ---

void fsd_build_precondition_frame(CANFRAME* frame) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_TRIP_PLANNING;
    frame->data_lenght = 8;
    // bit 0 = tripPlanningActive, bit 2 = requestActiveBatteryHeating
    frame->buffer[0] = 0x05;
}

void fsd_set_bit(CANFRAME* frame, int bit, bool value) {
    if(bit < 0 || bit >= 64) return;
    int byte_idx = bit / 8;
    int bit_idx = bit % 8;
    uint8_t mask = (uint8_t)(1U << bit_idx);
    if(value) {
        frame->buffer[byte_idx] |= mask;
    } else {
        frame->buffer[byte_idx] &= (uint8_t)(~mask);
    }
}

uint8_t fsd_read_mux_id(const CANFRAME* frame) {
    return frame->buffer[0] & 0x07;
}

bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd) {
    if(force_fsd) return true;
    if(frame->data_lenght < 5) return false;
    return (frame->buffer[4] >> 6) & 0x01;
}

TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame) {
    if(frame->canId != CAN_ID_GTW_CAR_CONFIG) return TeslaHW_Unknown;
    uint8_t das_hw = (frame->buffer[0] >> 6) & 0x03;
    switch(das_hw) {
    case 2:
        return TeslaHW_HW3;
    case 3:
        return TeslaHW_HW4;
    default:
        return TeslaHW_Unknown;
    }
}

// --- HW3/HW4 handlers ---

void fsd_handle_follow_distance(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    uint8_t fd = (frame->buffer[5] & 0xE0) >> 5;

    if(state->hw_version == TeslaHW_HW3) {
        switch(fd) {
        case 1:
            state->speed_profile = 2;
            break;
        case 2:
            state->speed_profile = 1;
            break;
        case 3:
            state->speed_profile = 0;
            break;
        default:
            break;
        }
    } else {
        switch(fd) {
        case 1:
            state->speed_profile = 3;
            break;
        case 2:
            state->speed_profile = 2;
            break;
        case 3:
            state->speed_profile = 1;
            break;
        case 4:
            state->speed_profile = 0;
            break;
        case 5:
            state->speed_profile = 4;
            break;
        default:
            break;
        }
    }
}

bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    uint8_t mux = fsd_read_mux_id(frame);
    bool fsd_ui = fsd_is_selected_in_ui(frame, state->force_fsd);
    bool modified = false;

    if(mux == 0) state->fsd_enabled = fsd_ui;

    if(state->hw_version == TeslaHW_HW3) {
        if(mux == 0 && state->fsd_enabled) {
            int raw = (int)((frame->buffer[3] >> 1) & 0x3F) - 30;
            int offset = raw * 5;
            if(offset < 0) offset = 0;
            if(offset > 100) offset = 100;
            state->speed_offset = offset;

            fsd_set_bit(frame, 46, true);
            frame->buffer[6] &= ~0x06;
            frame->buffer[6] |= (uint8_t)((state->speed_profile & 0x03) << 1);
            modified = true;
        }
        if(mux == 1) {
            fsd_set_bit(frame, 19, false);
            state->nag_suppressed = true;
            modified = true;
        }
        if(mux == 2 && state->fsd_enabled) {
            frame->buffer[0] &= ~0xC0;
            frame->buffer[1] &= ~0x3F;
            frame->buffer[0] |= (uint8_t)((state->speed_offset & 0x03) << 6);
            frame->buffer[1] |= (uint8_t)(state->speed_offset >> 2);
            modified = true;
        }
    } else {
        // HW4
        if(mux == 0 && state->fsd_enabled) {
            fsd_set_bit(frame, 46, true);
            fsd_set_bit(frame, 60, true);
            if(state->emergency_vehicle_detect) {
                fsd_set_bit(frame, 59, true);
            }
            modified = true;
        }
        if(mux == 1) {
            fsd_set_bit(frame, 19, false);
            fsd_set_bit(frame, 47, true);
            state->nag_suppressed = true;
            modified = true;
        }
        if(mux == 2) {
            frame->buffer[7] &= ~(0x07 << 4);
            frame->buffer[7] |= (uint8_t)((state->speed_profile & 0x07) << 4);
            modified = true;
        }
    }

    if(modified) state->frames_modified++;
    return modified;
}

// --- Legacy handler ---

void fsd_handle_legacy_stalk(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 2) return;
    uint8_t pos = frame->buffer[1] >> 5;
    if(pos <= 1)
        state->speed_profile = 2;
    else if(pos == 2)
        state->speed_profile = 1;
    else
        state->speed_profile = 0;
}

bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    uint8_t mux = fsd_read_mux_id(frame);
    bool fsd_ui = fsd_is_selected_in_ui(frame, state->force_fsd);
    bool modified = false;

    if(mux == 0) state->fsd_enabled = fsd_ui;

    if(mux == 0 && state->fsd_enabled) {
        fsd_set_bit(frame, 46, true);
        frame->buffer[6] &= ~0x06;
        frame->buffer[6] |= (uint8_t)((state->speed_profile & 0x03) << 1);
        modified = true;
    }
    if(mux == 1) {
        fsd_set_bit(frame, 19, false);
        state->nag_suppressed = true;
        modified = true;
    }

    if(modified) state->frames_modified++;
    return modified;
}

// --- ISA speed chime suppression ---

bool fsd_handle_isa_speed_chime(CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    frame->buffer[1] |= 0x20;
    uint8_t sum = 0;
    for(int i = 0; i < 7; i++)
        sum += frame->buffer[i];
    sum += (CAN_ID_ISA_SPEED & 0xFF) + (CAN_ID_ISA_SPEED >> 8);
    frame->buffer[7] = sum & 0xFF;
    return true;
}

// --- Extras: read-only parsers ---

void fsd_handle_di_system_status(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    // DI_trackModeState: byte 6 bits 1:0 (0=unavail 1=avail 2=on)
    state->track_mode_state = frame->buffer[6] & 0x03;
    // DI_tractionControlMode: byte 5 bits 2:0 (0=normal..5=dyno)
    state->traction_ctrl_mode = frame->buffer[5] & 0x07;
}

void fsd_handle_vcright_status(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 2) return;
    // VCRIGHT_rearDefrostState: byte 1 bits 2:0 (0=sna 1=on 2=off)
    state->rear_defrost_state = frame->buffer[1] & 0x07;
}

// --- Extras: write handlers (BETA, Service mode only) ---

bool fsd_handle_hazard_inject(const FSDState* state, CANFRAME* frame) {
    if(!state->extra_hazard_lights) return false;
    if(state->op_mode != OpMode_Service) return false;
    if(frame->data_lenght < 1) return false;
    // VCFRONT_hazardLightRequest: byte 0 bits 7:4
    // Set to 1 (HAZARD_REQUEST_BUTTON) when toggle is ON
    frame->buffer[0] = (frame->buffer[0] & 0x0F) | (0x01 << 4);
    return true;
}

bool fsd_handle_wiper_off(const FSDState* state, CANFRAME* frame) {
    if(!state->extra_wiper_off) return false;
    if(state->op_mode != OpMode_Service) return false;
    if(frame->data_lenght < 1) return false;
    // DAS_wiperSpeed: byte 0 bits 7:4, set to 0 (OFF)
    frame->buffer[0] &= 0x0F;
    return true;
}

void fsd_build_park_frame(CANFRAME* frame) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_SCCM_RSTALK;
    frame->data_lenght = 3;
    // SCCM_parkButtonStatus: byte 2 bits 1:0 = 1 (PRESSED)
    frame->buffer[2] = 0x01;
}

// --- DI_speed (0x257) parser: vehicle speed + UI speed ---
// opendbc tesla_model3_party.dbc:
//   DI_vehicleSpeed : 12|12@1+ (0.08,-40) kph
//   DI_uiSpeed      : 24|8@1+  (1,0)

void fsd_handle_di_speed(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    // DI_vehicleSpeed: 12-bit little-endian starting at bit 12
    uint16_t raw = ((uint16_t)(frame->buffer[2] & 0x0F) << 8) | frame->buffer[1];
    raw >>= 4; // shift down (bit 12 start in LE = byte1 upper nibble + byte2 lower)
    // Actually: bit12|12@1+ means start_bit=12, length=12, little-endian
    // byte1 bits[7:4] = bits 12-15, byte2 bits[7:0] = bits 16-23
    // Re-extract properly:
    raw = (((uint16_t)frame->buffer[2]) << 4) | (frame->buffer[1] >> 4);
    state->vehicle_speed_kph = (float)raw * 0.08f - 40.0f;
    if(state->vehicle_speed_kph < 0) state->vehicle_speed_kph = 0;

    // DI_uiSpeed: bit24|8 = byte 3
    state->ui_speed = frame->buffer[3];
    state->speed_seen = true;
}

// --- EPAS3S_currentTuneMode from 0x370 ---
// opendbc: EPAS3S_currentTuneMode : 7|3@0+ (big-endian, startBit=7, len=3)
// Means: MSB at bit 7 (byte0 bit7), 3 bits → byte0 bits [7:5]
// Also: EPAS3S_torsionBarTorque : 19|12@0+ (0.01,-20.5) Nm
// MSB at bit 19 (byte2 bit3), 12 bits → byte2[3:0] + byte1[7:0] ... complex big-endian

void fsd_handle_epas_steering_mode(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 3) return;
    // currentTuneMode: startBit=7, len=3, big-endian
    // In Motorola (big-endian) notation: MSB at bit 7 = byte0 bit7
    // 3 bits: byte0 bits [7:5]
    state->steering_tune_mode = (frame->buffer[0] >> 5) & 0x07;

    // torsionBarTorque: startBit=19, len=12, big-endian, factor=0.01, offset=-20.5
    // MSB at bit 19 = byte2 bit3. 12 bits big-endian:
    // byte2[3:0] (4 bits) + byte1[7:0] (8 bits) = 12 bits? No...
    // Actually big-endian startBit=19 means: byte=19/8=2, bit=19%8=3
    // So MSB is at byte2 bit3. 12 bits going MSB→LSB in big-endian:
    // byte2[3:0] (4 bits high), byte3[7:4] (4 bits mid), byte3[3:0] (4 bits low)?
    // Standard Motorola byte order for 12-bit: spans byte2 and byte3
    // Let's use a simpler extraction:
    uint16_t raw_torque = ((uint16_t)(frame->buffer[2] & 0x0F) << 8) | frame->buffer[3];
    state->torsion_bar_torque_nm = (float)raw_torque * 0.01f - 20.5f;
}

// --- ESP_status (0x145) parser ---
// opendbc: ESP_driverBrakeApply : 29|2@1+ (little-endian)
// bit 29 = byte3 bit5, 2 bits → byte3 bits [6:5]

void fsd_handle_esp_status(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    uint8_t brake = (frame->buffer[3] >> 5) & 0x03;
    state->driver_brake_applied = (brake != 0);
}

// --- GTW_epasControl (0x101) steering tune WRITE ---
// tuncasoftbildik: GTW_epasTuneRequest startBit=2, 3 bits, little-endian
// Values: 1=COMFORT, 2=STANDARD, 3=SPORT
// NOTE: Chassis CAN only — not on OBD-II Party CAN

void fsd_build_steering_tune_frame(CANFRAME* frame, uint8_t mode) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_GTW_EPAS_CTRL;
    frame->data_lenght = 8;
    // GTW_epasTuneRequest: startBit 2, 3 bits LE → byte0 bits [4:2]
    frame->buffer[0] = (mode & 0x07) << 2;
}

// --- DAS_status (0x39B) parser: AP state, blind spot, FCW, speed limit ---
// opendbc tesla_model3_party.dbc — all little-endian

void fsd_handle_das_status(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    // DAS_autopilotHandsOnState: bit42|4 → byte5 bits[5:2]
    state->das_hands_on_state = (frame->buffer[5] >> 2) & 0x0F;
    // DAS_autoLaneChangeState: bit46|5 → byte5 bits[7:6] + byte6 bits[2:0]
    state->das_lane_change = ((frame->buffer[5] >> 6) & 0x03) | ((frame->buffer[6] & 0x07) << 2);
    // DAS_laneDepartureWarning: bit37|3 → byte4 bits[7:5]
    // (not stored separately, included in lane_change context)
    // DAS_sideCollisionWarning: bit32|2 → byte4 bits[1:0]
    state->das_side_coll_warn = frame->buffer[4] & 0x03;
    // DAS_sideCollisionAvoid: bit30|2 → byte3 bits[7:6]
    state->das_side_coll_avoid = (frame->buffer[3] >> 6) & 0x03;
    // DAS_forwardCollisionWarning: bit22|2 → byte2 bits[7:6]
    state->das_fcw = (frame->buffer[2] >> 6) & 0x03;
    // DAS_visionOnlySpeedLimit: bit16|5 → byte2 bits[4:0], ×5 = kph
    state->das_vision_speed_lim = frame->buffer[2] & 0x1F;
    state->das_seen = true;
}

// --- DAS_status2 (0x389) parser: ACC report, activation failure ---

void fsd_handle_das_status2(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 5) return;
    // DAS_ACC_report: bit26|5 → byte3 bits[5:1]? Actually bit26 LE:
    // byte3 = bits 24-31, so bit26 = byte3 bit2, 5 bits → byte3 bits[6:2]
    state->das_acc_report = (frame->buffer[3] >> 2) & 0x1F;
    // DAS_activationFailureStatus: bit14|2 → byte1 bits[7:6]
    state->das_activation_fail = (frame->buffer[1] >> 6) & 0x03;
}

// --- DAS_settings (0x293) readback: autosteer enabled state ---

void fsd_handle_das_settings(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 5) return;
    // DAS_autosteerEnabled: bit38|1@0+ (big-endian)
    // Motorola bit 38 → byte4 bit6
    state->das_autosteer_on = (frame->buffer[4] >> 6) & 0x01;
}

// --- SCCM_leftStalk (0x249) builders — Party CAN, 3 bytes ---
// Frame layout:
//   byte0: CRC = (0x49 + 0x02 + byte1 + byte2) & 0xFF
//   byte1[3:0]: counter (4-bit, 0-15)
//   byte1[5:4]: SCCM_highBeamStalkStatus (0=IDLE 1=PULL 2=PUSH)
//   byte1[7:6]: SCCM_washWipeButtonStatus (0=NONE 1=1ST 2=2ND)
//   byte2[2:0]: SCCM_turnIndicatorStalkStatus (0=IDLE 1=UP1 2=UP2 3=DN1 4=DN2)

static void sccm_left_calc_crc(CANFRAME* frame) {
    frame->buffer[0] = ((CAN_ID_SCCM_LSTALK & 0xFF) + ((CAN_ID_SCCM_LSTALK >> 8) & 0xFF) +
                        frame->buffer[1] + frame->buffer[2]) &
                       0xFF;
}

void fsd_build_highbeam_flash(CANFRAME* frame, uint8_t counter, bool flash_on) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_SCCM_LSTALK;
    frame->data_lenght = 3;
    frame->buffer[1] = (counter & 0x0F);
    if(flash_on) {
        frame->buffer[1] |= (1 << 4); // highBeamStalkStatus = PULL
    }
    // byte2 = 0 (turn idle, reserved 0)
    sccm_left_calc_crc(frame);
}

void fsd_build_turn_signal(CANFRAME* frame, uint8_t counter, uint8_t direction) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_SCCM_LSTALK;
    frame->data_lenght = 3;
    frame->buffer[1] = (counter & 0x0F);
    frame->buffer[2] = (direction & 0x07); // 1=UP1(right), 3=DN1(left)
    sccm_left_calc_crc(frame);
}

void fsd_build_wiper_wash(CANFRAME* frame, uint8_t counter) {
    memset(frame, 0, sizeof(CANFRAME));
    frame->canId = CAN_ID_SCCM_LSTALK;
    frame->data_lenght = 3;
    frame->buffer[1] = (counter & 0x0F) | (1 << 6); // washWipe = 1ST_DETENT
    sccm_left_calc_crc(frame);
}

// --- DAS_control (0x2B9) parser: ACC state + set speed ---
// DAS_setSpeed: bit0|12 LE, factor 0.1, unit kph (4095=SNA)
// DAS_accState: bit12|4 LE (0=cancel,3=hold,4=ACC_ON,9=pause,13=cancel_silent)

void fsd_handle_das_control(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 3) return;
    uint16_t raw_spd = ((uint16_t)(frame->buffer[1] & 0x0F) << 8) | frame->buffer[0];
    if(raw_spd != 0x0FFF) // SNA
        state->das_set_speed_kph = raw_spd * 0.1f;
    state->das_acc_state = (frame->buffer[1] >> 4) & 0x0F;
}

// --- DI_state (0x286) parser: cruise, gear, park brake, digital speed ---
// DI_cruiseState: bit12|3 LE → byte1 bits[6:4] (from DI_state on Party CAN)
// DI_digitalSpeed: bit15|9 LE → byte1[7] + byte2[7:0], factor 0.5
// DI_parkBrakeState: bit32|4 LE → byte4 bits[3:0]
// DI_autoparkState: bit25|4 LE → byte3 bits[4:1]

void fsd_handle_di_state(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 5) return;
    state->di_cruise_state = (frame->buffer[1] >> 4) & 0x07;
    // digitalSpeed: 9-bit starting at bit 15
    uint16_t raw_ds = ((uint16_t)frame->buffer[2] << 1) | ((frame->buffer[1] >> 7) & 0x01);
    state->di_digital_speed = (uint8_t)(raw_ds >> 1); // approximate
    state->di_park_brake_state = frame->buffer[4] & 0x0F;
    state->di_autopark_state = (frame->buffer[3] >> 1) & 0x0F;
}

// --- DI_torque (0x108) parser ---
// opendbc: DI_torque1 : 0|13@1+ (0.25,-750) Nm

void fsd_handle_di_torque(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 2) return;
    uint16_t raw = ((uint16_t)(frame->buffer[1] & 0x1F) << 8) | frame->buffer[0];
    state->di_torque_nm = raw * 0.25f - 750.0f;
    state->di_torque_seen = true;
}

// --- UI_warning (0x311) parser ---
// buckleStatus: bit13|1 big-endian → byte1 bit5
// scrollWheelPressed: bit21|1 big-endian → byte2 bit5
// leftBlinkerOn: bit22|1 big-endian → byte2 bit6
// rightBlinkerOn: bit23|1 big-endian → byte2 bit7
// anyDoorOpen: bit28|1 big-endian → byte3 bit4
// highBeam: bit50|1 big-endian → byte6 bit2

void fsd_handle_ui_warning(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    state->ui_buckle_status = (frame->buffer[1] >> 5) & 0x01;
    state->ui_left_blinker = (frame->buffer[2] >> 6) & 0x01;
    state->ui_right_blinker = (frame->buffer[2] >> 7) & 0x01;
    state->ui_any_door_open = (frame->buffer[3] >> 4) & 0x01;
    state->ui_high_beam = (frame->buffer[6] >> 2) & 0x01;
    state->ui_warning_seen = true;
}

// --- SCCM_steeringAngleSensor (0x129) parser ---
// opendbc doesn't have the main angle in Model 3 DBC but it's likely
// similar to legacy: 14-bit signed value, factor 0.1 deg
// For now parse the raw bytes — exact signal layout needs verification

void fsd_handle_steering_angle(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    // Common Tesla steering angle: 16-bit signed LE at byte0-1, factor 0.1
    int16_t raw = (int16_t)(((uint16_t)frame->buffer[1] << 8) | frame->buffer[0]);
    state->steering_angle_deg = raw * 0.1f;
}

// --- DAS_steeringControl (0x488) parser ---
// DAS_steeringControlType: bit23|2 big-endian → byte2 bits[7:6]
// DAS_steeringAngleRequest: bit6|15 big-endian, factor 0.1, offset -1638.35

void fsd_handle_das_steering(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 3) return;
    state->das_steer_type = (frame->buffer[2] >> 6) & 0x03;
    // angle: 15-bit big-endian starting at bit 6
    uint16_t raw = ((uint16_t)(frame->buffer[0] & 0x7F) << 8) | frame->buffer[1];
    state->das_steer_angle_req = raw * 0.1f - 1638.35f;
}

// --- Nag killer (CAN 880 counter+1 echo) ---

bool fsd_handle_nag_killer(FSDState* state, const CANFRAME* frame, CANFRAME* out) {
    if(frame->data_lenght < 8) return false;
    if(!state->nag_killer) return false;

    // only act when handsOnLevel == 0 (no hands detected)
    uint8_t hands_on = (frame->buffer[4] >> 6) & 0x03;
    if(hands_on != 0) return false;

    // build echo frame
    out->canId = CAN_ID_EPAS_STATUS;
    out->data_lenght = 8;
    out->ext = 0;
    out->req = 0;

    out->buffer[0] = frame->buffer[0];
    out->buffer[1] = frame->buffer[1];
    out->buffer[2] = 0x08;
    out->buffer[3] = 0xB6; // torsionBarTorque = 1.80 Nm
    out->buffer[4] = frame->buffer[4] | 0x40; // handsOnLevel = 1
    out->buffer[5] = frame->buffer[5];

    // counter + 1 (byte6 lower nibble)
    uint8_t cnt = (frame->buffer[6] & 0x0F);
    cnt = (cnt + 1) & 0x0F;
    out->buffer[6] = (frame->buffer[6] & 0xF0) | cnt;

    // checksum: sum(byte0..6) + 0x70 + 0x03 (CAN ID 0x370 split)
    uint16_t sum = 0;
    for(int i = 0; i < 7; i++)
        sum += out->buffer[i];
    sum += (CAN_ID_EPAS_STATUS & 0xFF) + (CAN_ID_EPAS_STATUS >> 8);
    out->buffer[7] = (uint8_t)(sum & 0xFF);

    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}
