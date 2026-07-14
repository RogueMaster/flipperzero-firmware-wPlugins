#include "fsd_handler.h"
#include "fsd_checksum.h"
#include "fsd_can_ops.h"
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
    state->gtw_autopilot_tier = -1;
    state->das_hands_on_state = 0xFF; // unseen — nag killer echoes conservatively
    state->das_prev_hands_on_state = 0xFF; // escalation-edge baseline (#100)
    state->enhanced_autopilot = false;
    state->speed_profile_locked = false;
    state->hw4_offset = 0;
}

void fsd_handle_gtw_car_state(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    // GTW_updateInProgress: bits 1:0 of byte 6.
    // 0=No update, 1=Update available, 2=Installing, 3=Scheduled.
    // Only value 2 (installing) should suspend TX. Value 1 (available) caused
    // false positives on some firmware builds (issue #19).
    uint8_t raw = (frame->buffer[6] >> 0) & 0x03;
    bool in_progress = (raw == 2);
    if(in_progress) {
        state->tesla_ota_in_progress = true;
    } else {
        state->tesla_ota_in_progress = false;
    }
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
    tesla_set_bit(frame->buffer, bit, value);
}

uint8_t fsd_read_mux_id(const CANFRAME* frame) {
    return tesla_read_mux(frame->buffer);
}

bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd) {
    // Flipper has no china_mode field; pass false (behavior unchanged).
    return tesla_is_fsd_selected(frame->buffer, frame->data_lenght, force_fsd, false);
}

TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame) {
    if(frame->canId != CAN_ID_GTW_CAR_CONFIG) return TeslaHW_Unknown;
    // Some HW4 trims (Juniper/Giga) forward an all-zero 0x398 stub on the
    // gateway copy that reaches Bus 6 — only the mux byte ever moves. An empty
    // GTW_carConfig carries no real das_hw; reading it as 0 would mislabel the
    // car as Legacy and route it through the 0x3EE path. The real HW4 marker
    // lives on Vehicle CAN (0x39B). Treat an all-zero payload as Unknown so
    // detection falls through to the live markers instead of a stub.
    bool all_zero = true;
    for(uint8_t i = 0; i < frame->data_lenght && i < 8u; i++) {
        if(frame->buffer[i] != 0u) {
            all_zero = false;
            break;
        }
    }
    if(all_zero) return TeslaHW_Unknown;
    uint8_t das_hw = (frame->buffer[0] >> 6) & 0x03;
    switch(das_hw) {
    case 0:
    case 1:
        return TeslaHW_Legacy; // HW1/HW2/EAP retrofit — uses 0x3EE/0x045
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
    if(state->speed_profile_locked) return; // upstream: speedProfileLocked
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

bool fsd_ap_first_allows(const FSDState* state, uint32_t now_ms) {
    if(!state->ap_first) return true; // gate off -> always allow
    // 2 = AVAILABLE (AP offered, NOT engaged); 3 = ACTIVE_NOMINAL is the first
    // genuinely-engaged state. Injecting at 2 fired 0x3EE while AP was off (#108).
    if(state->das_ap_state < DAS_APSTATE_ENGAGED) return false; // AP not engaged yet
    if(state->ap_first_edge) return true; // experimental: inject at engage onset, no debounce
    // AP engaged: require it to have held stable for the debounce window.
    return (now_ms - state->ap_unstable_tick_ms) >= AP_FIRST_STABLE_MS;
}

bool fsd_soft_engage_allows(FSDState* state) {
    if(!state->soft_engage) return true; // gate off
    if(state->soft_engage_latched) return true; // already engaged this cycle
    // Hold until the wheel is near-centred so the DAS's path recompute at
    // FSD-enable is small (the steer-jerk is worse on curves — #108). If 0x129
    // isn't on the bus, steering_angle_deg stays 0 and this latches immediately
    // (degrades to AP-First-only, no worse than before).
    float a = state->steering_angle_deg;
    if(a < 0.0f) a = -a;
    if(a > SOFT_ENGAGE_ANGLE_DEG) return false; // turning — hold activation
    state->soft_engage_latched = true; // centred — begin and latch
    return true;
}

void fsd_abort_guard_update(FSDState* state) {
    if(!state->abort_guard) return;
    if(state->das_ap_state < DAS_APSTATE_ENGAGED) {
        state->abort_guard_latched = false; // clean disengage re-arms
    } else if(
        state->das_ap_state == DAS_APSTATE_ABORTING ||
        state->das_ap_state == DAS_APSTATE_ABORTED) {
        state->abort_guard_latched = true; // abort seen -> suppress injection
    }
}

bool fsd_abort_guard_allows(const FSDState* state) {
    return !(state->abort_guard && state->abort_guard_latched);
}

bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame, uint32_t now_ms) {
    if(frame->data_lenght < 8) return false;

    // AP-first (2026.14.x): don't modify 0x3FD until AP is engaged AND has been
    // stable for AP_FIRST_STABLE_MS — injecting on the activation edge is linked
    // to a steer-jerk (ev-open-can-tools#66 / v3.0.2-beta.2).
    if(!fsd_ap_first_allows(state, now_ms)) return false;
    // Soft Engage: additionally hold the first activation until the wheel is
    // centred, so FSD-enable's path recompute doesn't yank the steering (#108).
    if(!fsd_soft_engage_allows(state)) return false;
    // Abort Guard: once the car has entered an abort state this engagement, stop
    // injecting so we don't feed/repeat the abort that snaps the wheel (#108).
    if(!fsd_abort_guard_allows(state)) return false;
    // Minimal Inject: once the per-engagement burst budget is spent, stop modifying
    // for the rest of this engagement so injection stays at engage onset, off the
    // later abort edge. ap_inject_count is reset to 0 on disengage (#108).
    if(state->ap_first_minimal && state->ap_inject_count >= AP_MINIMAL_INJECT_FRAMES) return false;

    uint8_t mux = fsd_read_mux_id(frame);
    bool fsd_ui = fsd_is_selected_in_ui(frame, state->force_fsd);
    bool modified = false;

    if(mux == 0) state->fsd_enabled = fsd_ui;

    // bit38 explicit TLSSC enable on mux=0 (complementary to 0x331)
    if(mux == 0 && state->assist_tlssc_bit38 && state->fsd_enabled) {
        fsd_set_bit(frame, 38, true);
        modified = true;
    }

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
            if(state->enhanced_autopilot) {
                fsd_set_bit(frame, 46, true);
            }
            if(state->assist_show_lane_graph) {
                fsd_set_bit(frame, 45, true);
            }
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
            if(state->enhanced_autopilot) {
                fsd_set_bit(frame, 46, true);
            }
            if(state->assist_show_lane_graph) {
                fsd_set_bit(frame, 45, true);
            }
            state->nag_suppressed = true;
            modified = true;
        }
        if(mux == 2) {
            frame->buffer[7] &= ~(0x07 << 5);
            frame->buffer[7] |= (uint8_t)((state->speed_profile & 0x07) << 5);
            // HW4 speed offset runtime override
            // Source: ev-open-can-tools hw4OffsetRuntime
            if(state->hw4_offset > 0) {
                frame->buffer[1] = (frame->buffer[1] & 0xC0) | (state->hw4_offset & 0x3F);
            }
            modified = true;
        }
    }

    if(modified) {
        state->frames_modified++;
        if(state->ap_first_minimal) state->ap_inject_count++; // spend one burst frame (#108)
    }
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

bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame, uint32_t now_ms) {
    if(frame->data_lenght < 8) return false;

    // AP-first (timing safety): don't inject FSD-enable on 0x3EE until AP is
    // engaged and stable. The Legacy path was missing this gate; injecting on
    // the activation edge is linked to a steer-jerk on some Legacy/HW3 cars
    // (China FW 2026.8.3.6); see ev-open-can-tools#66. das_ap_state comes from
    // 0x399 DAS_status (Legacy/HW3).
    if(!fsd_ap_first_allows(state, now_ms)) return false;
    // Soft Engage: also hold the legacy activation until the wheel is centred (#108).
    if(!fsd_soft_engage_allows(state)) return false;
    // Abort Guard: stop injecting once an abort was seen this engagement (#108).
    if(!fsd_abort_guard_allows(state)) return false;
    // Minimal Inject: stop modifying once this engagement's burst budget is spent,
    // so injection lands at engage onset, not the later abort edge (#108).
    if(state->ap_first_minimal && state->ap_inject_count >= AP_MINIMAL_INJECT_FRAMES) return false;

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

    if(modified) {
        state->frames_modified++;
        if(state->ap_first_minimal) state->ap_inject_count++; // spend one burst frame (#108)
    }
    return modified;
}

// --- ISA speed chime suppression ---

bool fsd_handle_isa_speed_chime(CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    frame->buffer[1] |= 0x20;
    frame->buffer[7] = tesla_additive_checksum(CAN_ID_ISA_SPEED, frame->buffer, 7);
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
    if(frame->data_lenght < 4) return;
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
// Values: 0=NotInit_orOff, 1=Not_Applied, 2=Driver_applying_brakes, 3=Faulty_SNA

void fsd_handle_esp_status(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    uint8_t brake = (frame->buffer[3] >> 5) & 0x03;
    state->driver_brake_applied = (brake >= 2);
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

// --- DAS_status parser: AP state, blind spot, FCW, speed limit ---
//
// HW-dependent CAN ID + byte layout:
//   Pre-Highland HW3 / Legacy: 0x399 (legacy CAN map per opendbc/tesla_can.dbc)
//   Highland HW3 / HW4:        0x39B (party CAN map per opendbc/tesla_model3_party.dbc)
//
// HW3 layout (0x399, legacy):
//   byte 0 low nibble = DAS_autopilotState   (2=available, 3=engaged)
//   byte 5 bits[5:2]  = DAS_handsOnState     (matches HW4 position)
//
// HW4 layout (0x39B, party): full multi-signal parser below.
//
// v2.14 silently broke pre-Highland HW3 users by reading 0x39B only.
// Caller dispatches by hw_version in scenes/fsd_running.c.

void fsd_handle_das_status_hw3(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    state->das_ap_state = frame->buffer[0] & 0x0Fu;
    state->das_hands_on_state = (frame->buffer[5] >> 2) & 0x0Fu;
    state->das_seen = true;
}

void fsd_handle_das_status_hw4(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 7) return;
    // DAS_autopilotState: bit12|4 → byte1 bits[7:4]
    // 0=UNAVAIL 1=UNAVAILABLE/AVAIL-flicker 2=AVAILABLE (offered, not engaged)
    // 3=ACTIVE_NOMINAL (first engaged) 4=ACTIVE_MIN_DRIVER 6=active 8/9=aborting/aborted
    uint8_t hw4_state = (frame->buffer[1] >> 4) & 0x0F;
    // HW4 Highland (China MIC, fw 2026.20) carries DAS_autopilotState in byte0 low
    // nibble (HW3 position: 1=avail 2=ready 3=engaged) while byte1[7:4] is pinned at
    // 1 the whole drive (#116). Latch to byte0 only on the unique signature — byte0
    // active (>=2) while byte1[7:4] stays exactly 1 across 3 frames — and never once
    // byte1[7:4] is seen != 1 (that proves a standard HW4 car, byte0 left untouched).
    uint8_t b0_state = frame->buffer[0] & 0x0F;
    if(hw4_state != 1u) {
        state->das_hw4_byte1_moved = true;
        state->das_hw4_byte0_pin_count = 0;
    } else if(!state->das_hw4_use_byte0 && !state->das_hw4_byte1_moved && b0_state >= 2u) {
        if(state->das_hw4_byte0_pin_count < 3u) state->das_hw4_byte0_pin_count++;
        if(state->das_hw4_byte0_pin_count >= 3u) state->das_hw4_use_byte0 = true;
    }
    state->das_ap_state = state->das_hw4_use_byte0 ? b0_state : hw4_state;
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
    state->das_hw4_status_seen = true;
}

// HW4 0x399 hands-on fallback.
//
// Some HW4 trims (observed on a Juniper RWD, Bus 6 / X179 pin 13/14, #100)
// never broadcast 0x39B; on those cars 0x399 carries the hands-on escalation in
// the SAME byte5 bits[5:2] field as 0x39B (verified against a captured nag run:
// the field steps 1→2→3 as the visual nag escalates). When 0x39B has not been
// seen, read just that field from 0x399 so the nag gate isn't starved.
//
// Deliberately reads ONLY the hands-on field — not das_ap_state — because the
// 0x399 byte0 layout on HW4 is not confirmed (0x399 is the ISA chime there).
void fsd_handle_das_handsonly_399(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    state->das_hands_on_state = (frame->buffer[5] >> 2) & 0x0Fu;
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
    state->das_autosteer_on = (frame->buffer[4] >> 6) & 0x01;
}

// --- GTW_carConfig (0x7FF / 2047) mux=2 — autopilot tier ---
// Source: ev-open-can-tools readGTWAutopilot()
// byte[5] bits 4:2 → 0=NONE 1=HIGHWAY 2=ENHANCED 3=SELF_DRIVING 4=BASIC

void fsd_handle_gtw_autopilot_tier(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    uint8_t mux = frame->buffer[0] & 0x07;
    if(mux != 2) return;
    state->gtw_autopilot_tier = (int8_t)((frame->buffer[5] >> 2) & 0x07);
}

// --- 0x7FF GTW Config Replay (formerly "Ban Shield") ---
//
// Phase 1 (NOT armed): learn the "healthy" 0x7FF state by capturing
// each mux frame. Once all 8 muxes are seen, the snapshot is complete
// and the replayer auto-arms.
//
// Phase 2 (armed): compare every incoming 0x7FF against the snapshot.
// If ANY byte differs, replay the snapshot bytes into the frame and
// return true — the caller retransmits immediately, racing the
// gateway's modified frame so the AP ECU sees the learned-healthy
// version.
//
// This is a CAN-broadcast-layer mask only. It does not undo NVRAM
// changes the gateway has already written, nor any backend-side
// entitlement flags. Honest framing per #60.

bool fsd_handle_gtw_shield(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    uint8_t mux = frame->buffer[0] & 0x07;

    if(!state->gtw_shield_armed) {
        // Learning phase: capture snapshot
        if(!state->gtw_snapshot_valid[mux]) {
            for(int i = 0; i < 8; i++)
                state->gtw_snapshot[mux][i] = frame->buffer[i];
            state->gtw_snapshot_valid[mux] = true;

            // Auto-arm once all 8 muxes are captured
            bool all_valid = true;
            for(int m = 0; m < 8; m++) {
                if(!state->gtw_snapshot_valid[m]) {
                    all_valid = false;
                    break;
                }
            }
            if(all_valid) state->gtw_shield_armed = true;
        }
        return false;
    }

    // Armed: compare against snapshot
    if(!state->gtw_snapshot_valid[mux]) return false;

    bool changed = false;
    for(int i = 0; i < 8; i++) {
        if(frame->buffer[i] != state->gtw_snapshot[mux][i]) {
            changed = true;
            break;
        }
    }

    if(changed) {
        // Overwrite with healthy snapshot
        for(int i = 0; i < 8; i++)
            frame->buffer[i] = state->gtw_snapshot[mux][i];
        state->gtw_shield_blocks++;
        return true; // caller should retransmit
    }

    return false;
}

// --- 0x7FF Active Tier Override ---
// Force GTW_autopilot to SELF_DRIVING (3) on every mux=2 frame.
// More aggressive than GTW Config Replay — doesn't just replay learned state,
// actively writes tier=3 regardless of what the gateway broadcasts.
// Source: Shayennn/FUCKYOU-TESLA-FSD vehicle_logic.h

bool fsd_handle_gtw_tier_override(FSDState* state, CANFRAME* frame) {
    if(!state->gtw_tier_override) return false;
    if(frame->data_lenght < 6) return false;
    uint8_t mux = frame->buffer[0] & 0x07;
    if(mux != 2) return false;

    // byte[5] bits 4:2 = autopilot tier. Set to 3 (SELF_DRIVING).
    uint8_t original = frame->buffer[5];
    uint8_t modified = (original & ~0x1C) | (3 << 2);
    if(modified == original) return false;

    frame->buffer[5] = modified;
    return true;
}

// --- 0x3F8 Driver Assist Override ---
// Region unlock, nav FSD, hands-off, dev mode, driving side.
// Source: Shayennn/FUCKYOU-TESLA-FSD HW3Handler/HW4Handler

bool fsd_handle_driver_assist_override(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    bool modified = false;

    // bit5: UI_dasDeveloper
    if(state->assist_dev_mode) {
        fsd_set_bit(frame, 5, true);
        modified = true;
    }
    // bit13: UI_driveOnMapsEnable
    // bit48: UI_hasDriveOnNav
    // bit49: UI_followNavRouteEnable
    if(state->assist_nav_enable) {
        fsd_set_bit(frame, 13, true);
        fsd_set_bit(frame, 48, true);
        fsd_set_bit(frame, 49, true);
        modified = true;
    }
    // bit14: UI_handsOnRequirementDisable
    if(state->assist_hands_off) {
        fsd_set_bit(frame, 14, true);
        modified = true;
    }
    // bit40-41: UI_drivingSide = 1 (LHD)
    if(state->assist_lhd_override) {
        fsd_set_bit(frame, 40, true);
        fsd_set_bit(frame, 41, false);
        modified = true;
    }
    // bit43: UI_enableTripTelemetry = 0 (disable trip data collection)
    if(state->assist_telemetry_off) {
        fsd_set_bit(frame, 43, false);
        modified = true;
    }

    return modified;
}

// --- 0x33A Energy Consumption Parser ---

void fsd_handle_energy_consumption(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 4) return;
    uint16_t raw = ((uint16_t)frame->buffer[1] << 8) | frame->buffer[0];
    state->energy_wh_per_km = raw * 0.1f;
    state->energy_seen = true;
}

// --- TLSSC Restore (0x331 / 817) ---
// Spoof DAS_autopilot + DAS_autopilotBase to SELF_DRIVING.
// byte[0] lower 6 bits → 0x1B. Preserves upper 2 bits.
// Source: community research in issue #18 (gauner1986, kp43h8, MiniCS).

bool fsd_handle_tlssc_restore(FSDState* state, CANFRAME* frame) {
    if(!state->tlssc_restore) return false;
    if(frame->data_lenght < 1) return false;

    uint8_t original = frame->buffer[0];
    uint8_t modified = (original & 0xC0) | 0x1B;

    if(modified == original) return false;

    frame->buffer[0] = modified;
    state->tlssc_restore_count++;
    return true;
}

// --- Track Mode inject (0x313 / 787) ---
// Source: ev-open-can-tools HW3Handler frame.id == 787
// byte[0] bits 1:0 = 0x01 (kTrackModeRequestOn)
// checksum in byte[7] = computeVehicleChecksum

bool fsd_handle_track_mode_inject(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    if(state->op_mode != OpMode_Service) return false;
    if(state->track_mode_state == 0) return false; // require explicit user toggle
    // set track mode request ON
    frame->buffer[0] = (frame->buffer[0] & 0xFC) | 0x01;
    // recalculate Tesla vehicle checksum
    frame->buffer[7] = tesla_additive_checksum(CAN_ID_TRACK_MODE_SET, frame->buffer, 7);
    return true;
}

// --- Scroll-Press AP Engage (0x3C2, HW4-only) ---
//
// Per @JakNo's bench testing in #43: injecting the right-scrollwheel-down
// sequence on VCLEFT_switchStatus mux=1 engages AP and the car treats it as
// indistinguishable from a physical scrollwheel press. No counter, no CRC —
// just replace the swcRightPressed bit-pair in 4 consecutive mux=1 frames.
//
// State machine across 0x3C2 mux=1 frames:
//   state=0, armed=false  : initial; wait for das_ap_state==0 before arming
//   state=0, armed=true   : armed; will fire when das_ap_state transitions to 1
//   state=1..4            : actively firing frame N of the sequence
//   state=5               : cooldown; wait for das_ap_state==0 before re-arming
//
// Sequence: swcRightPressed = 1, 2, 2, 1 across 4 consecutive frames.
// Field is bits 12-13 of the 64-bit frame = byte 1 bits 4-5.

// VCLEFT_switchStatus mux=1 signal positions (opendbc tesla_model3_vehicle.dbc):
//   swcRightPressed    : startbit 12, 2 bits  → byte 1 bits 4-5
//   swcRightScrollTicks: startbit 24, 6 bits signed → byte 3 bits 0-5
#define SCROLL_SWC_PRESSED_PRESSED 1u // "pressed" value (per @JakNo bench; pending re-confirm)
#define SCROLL_SWC_SCROLLTICKS_UP \
    1u // one detent up; 6-bit signed (+1). Direction pending @JakNo confirm
// Phase durations per @JakNo's #82 flow (milliseconds, approximate — tune on-car)
#define SCROLL_T_PRESS1_MS  250u
#define SCROLL_T_SCROLL1_MS 150u
#define SCROLL_T_PRESS2_MS  250u

static void scroll_set_pressed(CANFRAME* frame, uint8_t v) {
    frame->buffer[1] = (frame->buffer[1] & ~0x30u) | ((v & 0x03u) << 4);
}

static void scroll_set_scrollticks(CANFRAME* frame, uint8_t v) {
    frame->buffer[3] = (frame->buffer[3] & ~0x3Fu) | (v & 0x3Fu);
}

bool fsd_handle_scroll_press_inject(FSDState* state, CANFRAME* frame, uint32_t now_ms) {
    if(!state->scroll_press_ap) return false;
    if(state->hw_version != TeslaHW_HW4) return false; // HW4-only per v2.15 scope
    if(state->op_mode != OpMode_Service) return false; // Service mode safety gate
    if(frame->data_lenght < 4) return false; // need byte 3 for scrollTicks

    // VCLEFT_switchStatusIndex (mux) at byte 0 bits 0-1. Right-scroll lives on mux=1.
    uint8_t mux = frame->buffer[0] & 0x03;
    if(mux != 1) return false;

    uint8_t ap = state->das_ap_state;

    // Arm tracking: require a UNAVAIL observation before any fire, then before each re-fire.
    if(ap == 0) {
        state->scroll_press_armed = true;
        if(state->scroll_press_state == 5) {
            state->scroll_press_state = 0; // cooldown cleared, ready to re-arm
        }
    }

    // Rising-edge fire trigger: AP UNAVAIL(0)→AVAIL(1) while armed.
    if(state->scroll_press_state == 0 && state->scroll_press_armed && ap == 1) {
        state->scroll_press_state = 1; // enter phase 1 (press1)
        state->scroll_press_armed = false;
        state->scroll_press_phase_ms = now_ms;
    }

    if(state->scroll_press_state < 1 || state->scroll_press_state > 4) {
        return false;
    }

    uint32_t elapsed = now_ms - state->scroll_press_phase_ms;
    bool modified = false;

    switch(state->scroll_press_state) {
    case 1: // 1st press, hold for ~250 ms
        scroll_set_pressed(frame, SCROLL_SWC_PRESSED_PRESSED);
        modified = true;
        if(elapsed >= SCROLL_T_PRESS1_MS) {
            state->scroll_press_state = 2;
            state->scroll_press_phase_ms = now_ms;
        }
        break;
    case 2: // scroll up, hold for ~150 ms
        scroll_set_scrollticks(frame, SCROLL_SWC_SCROLLTICKS_UP);
        modified = true;
        if(elapsed >= SCROLL_T_SCROLL1_MS) {
            state->scroll_press_state = 3;
            state->scroll_press_phase_ms = now_ms;
        }
        break;
    case 3: // 2nd press, hold for ~250 ms
        scroll_set_pressed(frame, SCROLL_SWC_PRESSED_PRESSED);
        modified = true;
        if(elapsed >= SCROLL_T_PRESS2_MS) {
            state->scroll_press_state = 4;
            state->scroll_press_phase_ms = now_ms;
        }
        break;
    case 4: // final scroll up, single frame, then cooldown
        scroll_set_scrollticks(frame, SCROLL_SWC_SCROLLTICKS_UP);
        modified = true;
        state->scroll_press_state = 5; // cooldown until AP drops to UNAVAIL
        break;
    default:
        return false;
    }

    if(modified) state->frames_modified++;
    return modified;
}

// --- SCCM_leftStalk (0x249) builders — Party CAN, 3 bytes ---
// Frame layout:
//   byte0: CRC = (0x49 + 0x02 + byte1 + byte2) & 0xFF
//   byte1[3:0]: counter (4-bit, 0-15)
//   byte1[5:4]: SCCM_highBeamStalkStatus (0=IDLE 1=PULL 2=PUSH)
//   byte1[7:6]: SCCM_washWipeButtonStatus (0=NONE 1=1ST 2=2ND)
//   byte2[2:0]: SCCM_turnIndicatorStalkStatus (0=IDLE 1=UP1 2=UP2 3=DN1 4=DN2)

static void sccm_left_calc_crc(CANFRAME* frame) {
    frame->buffer[0] = tesla_additive_checksum(CAN_ID_SCCM_LSTALK, &frame->buffer[1], 2);
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

// --- Nag killer (DAS-aware counter+1 echo) ---
//
// Improved from ev-open-can-tools PR #5 (zdenekbouresh):
//
// 1. DAS-aware gating: only echo when DAS_autopilotHandsOnState (from
//    0x39B/0x399, already parsed in fsd_handle_das_status_hw3/hw4) indicates the car
//    is actually demanding hands-on. States 0 (NOT_REQD) and 8
//    (SUSPENDED) mean DAS is satisfied — no echo needed. This reduces
//    spurious bus traffic from ~25 frames/sec to near-zero during normal
//    driving.
//
// 2. Organic torque variation: replaces fixed 1.80 Nm with a smooth
//    random walk [1.00-2.40 Nm] plus brief "grip pulses" [3.10-3.30 Nm]
//    every ~5-9 seconds. A flat torque signal for 30+ minutes is a
//    statistical impossibility from a real hand — this makes telemetry
//    detection much harder.

// xorshift32 PRNG — no stdlib dependency, deterministic, fast
static uint32_t nag_prng_state = 0xDEADBEEF;
static uint32_t nag_xorshift32(void) {
    uint32_t x = nag_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    nag_prng_state = x;
    return x;
}

// Torque random walk state
static int16_t nag_torq_walk = 2230; // raw starting = 1.80 Nm
static uint8_t nag_exc_frames = 0; // frames in grip excursion
static uint16_t nag_frames_until_exc = 175; // frames until next excursion
// ── EPAS-faithful (Mode-C) nag path (v2.17, #100) ───────────────────────────
// A demand-state-driven steering-torque model ported from the nicolozak
// "Mode C" reference (surfaced by @ssw0209-sys, #100), which works in the wild
// on 2026.14.x. Unlike the legacy killer (forces handsOnLevel=1 + steady torque),
// this derives handsOnLevel FROM the synthetic torque magnitude so the frame is
// internally consistent, escalates torque with DAS hands-on demand, and directs
// it opposite the steering angle. Raw torque: 2048 = 0 Nm, raw = 2048 + Nm*100.
static uint8_t nag_hands_level_from_raw(int16_t raw) {
    int16_t a = (raw >= 2048) ? (raw - 2048) : (2048 - raw);
    if(a >= 200) return 2; // |torque| >= 2.0 Nm
    if(a >= 100) return 1; // |torque| >= 1.0 Nm
    return 0;
}

// Clamp a torsionBarTorque raw value to the ±1.8 Nm safety cap (#122).
static int16_t nag_clamp_torque(int16_t raw) {
    if(raw > NAG_TORQUE_RAW_MAX) return NAG_TORQUE_RAW_MAX;
    if(raw < NAG_TORQUE_RAW_MIN) return NAG_TORQUE_RAW_MIN;
    return raw;
}

static bool
    nag_faithful_modec(FSDState* state, const CANFRAME* frame, CANFRAME* out, uint32_t now_ms) {
    uint8_t das = state->das_hands_on_state; // DAS hands-on demand state

    // ── transition memory ──
    static uint8_t prev_das = 0xFF;
    static uint32_t s1_enter_ms = 0;
    static uint32_t s2_enter_ms = 0;
    static uint32_t strong_enter_ms = 0;
    static int16_t last_raw = 2048;
    static uint8_t last_level = 0;
    static uint32_t s2_hold_until_ms = 0;
    static int16_t s2_hold_raw = 2048;
    static uint8_t s2_hold_level = 0;
    static bool s2_level2_active = false;
    static int16_t mild_walk = 2048;

    bool is_strong = (das == 3 || das == 4 || das == 5);
    bool prev_strong = (prev_das == 3 || prev_das == 4 || prev_das == 5);
    if(prev_das != 1 && das == 1) {
        s1_enter_ms = now_ms;
    }
    if(das != 1) {
        s1_enter_ms = 0;
    }
    if(prev_das != 2 && das == 2) {
        s2_enter_ms = now_ms;
    }
    if(das != 2) {
        s2_enter_ms = 0;
        s2_hold_until_ms = 0;
        s2_level2_active = false;
    }
    if(!prev_strong && is_strong) {
        strong_enter_ms = now_ms;
    }
    if(!is_strong) {
        strong_enter_ms = 0;
    }
    prev_das = das;

    // Global gate: AP active, and demand state not satisfied/suspended.
    // NOTE: this < 2u is the nag-echo gate, NOT the AP-First engagement threshold
    // (DAS_APSTATE_ENGAGED=3). Nag suppression is intentionally relevant down to
    // AVAILABLE, so this stays at < 2u by design (#108).
    if(state->das_ap_state < 2u || das == 0 || das == 8 || das == 15) {
        last_raw = 2048;
        last_level = 0;
        return false; // pass the real EPAS frame through unmodified
    }

    int dir = (state->steering_angle_deg > 0.0f) ? -1 : 1; // oppose steering
    int16_t torque;
    uint8_t level;

    if(das == 1) {
        // Idle, but hold the last injected value for a 500 ms grace after dropping
        // from an active demand state to avoid an abrupt cutoff.
        if(s1_enter_ms != 0 && (now_ms - s1_enter_ms) < 500u) {
            torque = last_raw;
            level = last_level;
        } else {
            last_raw = 2048;
            last_level = 0;
            return false;
        }
    } else if(das == 2) {
        // Mild demand: 2 s idle delay, then a mild random walk in the band
        // opposite the steering angle.
        if(s2_enter_ms != 0 && (now_ms - s2_enter_ms) < 2000u) return false;
        if(now_ms < s2_hold_until_ms) {
            torque = s2_hold_raw;
            level = s2_hold_level;
        } else {
            int16_t lo = (dir < 0) ? 1848 : 2098; // -2.0 Nm..-0.5 Nm / +0.5..+2.0
            int16_t hi = (dir < 0) ? 1998 : 2248;
            if(mild_walk < lo || mild_walk > hi) mild_walk = (int16_t)((lo + hi) / 2);
            mild_walk += (int16_t)((nag_xorshift32() % 25u) - 12); // ±12/frame
            if(mild_walk < lo) mild_walk = lo;
            if(mild_walk > hi) mild_walk = hi;
            torque = mild_walk;
            level = nag_hands_level_from_raw(torque);
            int16_t a = (torque >= 2048) ? (torque - 2048) : (2048 - torque);
            bool l2 = (a >= 200);
            if(l2 && !s2_level2_active) { // latch first level-2 crossing for 1 s
                s2_hold_until_ms = now_ms + 1000u;
                s2_hold_raw = torque;
                s2_hold_level = 2;
            }
            s2_level2_active = l2;
        }
    } else {
        // Strong demand (3/4/5): 1 s pause, then ramp 0->2.1 Nm over 500 ms and
        // hold, on a 1.5 s cycle, opposite the steering angle.
        if(strong_enter_ms != 0 && (now_ms - strong_enter_ms) < 1000u) return false;
        uint32_t active_ms = (strong_enter_ms == 0) ? 0u : (now_ms - strong_enter_ms - 1000u);
        uint16_t phase = (uint16_t)(active_ms % 1500u);
        int16_t mag = 210; // 2.1 Nm
        if(phase < 500u) mag = (int16_t)(((int32_t)phase * 210) / 500);
        torque = (int16_t)(2048 + dir * mag);
        level = nag_hands_level_from_raw(torque);
    }

    last_raw = torque;
    last_level = level;

    out->canId = CAN_ID_EPAS_STATUS;
    out->data_lenght = 8;
    out->ext = 0;
    out->req = 0;
    out->buffer[0] = frame->buffer[0];
    out->buffer[1] = frame->buffer[1];
    torque = nag_clamp_torque(torque); // ±1.8 Nm safety cap (#122)
    out->buffer[2] = (frame->buffer[2] & 0xF0) | (uint8_t)((torque >> 8) & 0x0F);
    out->buffer[3] = (uint8_t)(torque & 0xFF);
    // Leave handsOnLevel untouched — real EPAS keeps byte4[7:6] at 0 even when
    // hands are genuinely on (verified on HW3 14.6 clean-nag 142/142=0 and HW4
    // Feifan, #122). Deriving it from torque is non-EPAS-like and a likely
    // 14.x preflight tell. (level is still tracked for the state-1 grace hold.)
    (void)level;
    out->buffer[4] = frame->buffer[4];
    out->buffer[5] = frame->buffer[5];
    uint8_t cnt = (frame->buffer[6] & 0x0F);
    cnt = (cnt + 1) & 0x0F;
    out->buffer[6] = (frame->buffer[6] & 0xF0) | cnt;
    out->buffer[7] = tesla_additive_checksum(CAN_ID_EPAS_STATUS, out->buffer, 7);
    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}

void fsd_apply_signal_config(FSDState* state, const CANFRAME* frame, uint32_t now_ms) {
    if(state->cfg_das_id != 0 && frame->canId == state->cfg_das_id) {
        if(state->cfg_apstate_byte < 8 && frame->data_lenght > state->cfg_apstate_byte)
            state->das_ap_state =
                (frame->buffer[state->cfg_apstate_byte] >> state->cfg_apstate_shift) &
                state->cfg_apstate_mask;
        if(state->cfg_handson_byte < 8 && frame->data_lenght > state->cfg_handson_byte)
            state->das_hands_on_state =
                (frame->buffer[state->cfg_handson_byte] >> state->cfg_handson_shift) &
                state->cfg_handson_mask;
        state->das_ctx_seen_ms = now_ms;
    }
    if(state->cfg_steer_id != 0 && frame->canId == state->cfg_steer_id) {
        if(state->cfg_steer_hi < 8 && state->cfg_steer_lo < 8 &&
           frame->data_lenght > state->cfg_steer_hi && frame->data_lenght > state->cfg_steer_lo) {
            int16_t raw = (int16_t)(((uint16_t)frame->buffer[state->cfg_steer_hi] << 8) |
                                    frame->buffer[state->cfg_steer_lo]);
            state->steering_angle_deg = (float)raw * 0.1f;
            state->steer_ctx_seen_ms = now_ms;
        }
    }
}

bool fsd_das_ctx_fresh(const FSDState* state, uint32_t now_ms) {
    if(state->cfg_das_id == 0) return true; // auto mode — prior behaviour
    return (now_ms - state->das_ctx_seen_ms) <= NAG_CTX_FRESH_MS;
}

// Burst/pause window: true while we should be RESTING (skip the echo). #122.
static bool nag_in_pause(uint32_t now_ms) {
    uint32_t cycle = NAG_BURST_MS + NAG_PAUSE_MS;
    return (now_ms % cycle) >= NAG_BURST_MS;
}

bool fsd_handle_nag_killer(FSDState* state, const CANFRAME* frame, CANFRAME* out, uint32_t now_ms) {
    if(frame->data_lenght < 8) return false;
    if(!state->nag_killer) return false;
    // Freshness: with a custom DAS source configured, no-op if it's stale (#122).
    if(!fsd_das_ctx_fresh(state, now_ms)) return false;
    // Burst/pause: during the rest window, don't echo at all (gates both paths).
    if(state->nag_burst && nag_in_pause(now_ms)) return false;

    // v2.17 EPAS-faithful (Mode-C) path takes over the whole decision.
    if(state->nag_epas_faithful) return nag_faithful_modec(state, frame, out, now_ms);

    // Act on handsOnLevel 0 (nag imminent) and 3 (escalated alarm).
    // Previous "hands_on != 0" guard silently skipped level 3, leaving the
    // escalated alarm unsuppressed. Only skip when hands are actually
    // detected (level 1).
    uint8_t hands_on = (frame->buffer[4] >> 6) & 0x03;
    if(hands_on == 1) return false;

    // DAS-aware gating: skip echo when DAS is satisfied or AP suspended.
    // das_hands_on_state is parsed from 0x39B (HW4) or 0x399 (HW3) in
    // fsd_handle_das_status_hw4 / fsd_handle_das_status_hw3.
    // 0 = NOT_REQD (satisfied), 8 = SUSPENDED (AP paused).
    // 0xFF = no DAS frame seen yet — echo conservatively as fallback.
    uint8_t das = state->das_hands_on_state;
    // Track das across every frame (including the satisfied/suspended early-return
    // below) so a 0 (NOT_REQD) wave resets the escalation baseline and the next
    // 0->2 jump still registers as a rising edge. Source: #100.
    uint8_t das_prev = state->das_prev_hands_on_state;
    state->das_prev_hands_on_state = das;
    if(das == 0 || das == 8) return false;

    // On-demand grip pulse: fire an immediate excursion when handsOnLevel
    // rises into a nag-demand state (0=imminent, 3=escalated). v2.14 only
    // emitted grip pulses on a fixed 5-9 s schedule, which let the yellow
    // 2-second escalation get there first when a nag arrived between pulses.
    // Resets the periodic cooldown so we don't double-pulse.
    // Source: @deftdawg variant tested on 2016 MX HW3 in #70.
    //
    // Also re-arm on a DAS escalation edge: on some HW4 trims (#100) EPAS
    // handsOnLevel (byte4) is frozen at 0 the whole hands-off window, so the only
    // nag signal that moves is das_hands_on_state stepping 0->2->3. Without this
    // the pulse fires once and never re-arms, letting the 2nd wave escalate to
    // yellow (das 0/8 already returned above; 0xFF = unseen).
    // Source: @jewelrylin / @DrStrangeglovebox on-car data in #100.
    bool das_escalation = (das != 0xFF) && (das_prev == 0xFF || das > das_prev);
    bool demand_now = (hands_on == 0 || hands_on == 3);
    if((das_escalation || (demand_now && !state->nag_demand_active)) && nag_exc_frames == 0) {
        nag_exc_frames = 3 + (nag_xorshift32() % 3); // 3-5 frame pulse
        nag_frames_until_exc = 125 + (nag_xorshift32() % 100); // reset 5-9 s cooldown
    }
    state->nag_demand_active = demand_now;

    // --- Organic torque variation ---
    // torsionBarTorque encoding: tRaw = (Nm + 20.5) / 0.01
    // d[2] lower nibble = tRaw >> 8, d[3] = tRaw & 0xFF
    int16_t torq;
    if(nag_exc_frames > 0) {
        // Grip pulse: ~3.20 Nm ± small noise
        torq = 2350 + (int16_t)((nag_xorshift32() % 41) - 20);
        nag_exc_frames--;
    } else {
        // Normal random walk: step ±15 per frame
        int16_t step = (int16_t)((nag_xorshift32() % 31) - 15);
        nag_torq_walk += step;
        if(nag_torq_walk < 2150) nag_torq_walk = 2150; // min ~1.00 Nm
        if(nag_torq_walk > 2290) nag_torq_walk = 2290; // max ~2.40 Nm
        torq = nag_torq_walk;

        // Count down to next grip excursion
        if(nag_frames_until_exc > 0) {
            nag_frames_until_exc--;
        } else {
            nag_exc_frames = 3 + (nag_xorshift32() % 3); // 3-5 frames
            nag_frames_until_exc = 125 + (nag_xorshift32() % 100); // 5-9 sec
        }
    }

    // build echo frame
    out->canId = CAN_ID_EPAS_STATUS;
    out->data_lenght = 8;
    out->ext = 0;
    out->req = 0;

    out->buffer[0] = frame->buffer[0];
    out->buffer[1] = frame->buffer[1];
    torq = nag_clamp_torque(torq); // ±1.8 Nm safety cap (#122)
    out->buffer[2] = (frame->buffer[2] & 0xF0) | (uint8_t)((torq >> 8) & 0x0F);
    out->buffer[3] = (uint8_t)(torq & 0xFF);
    // Clear existing handsOnLevel bits (7:6) before setting level=1.
    // OR-ing 0x40 without clearing leaves level=3 unchanged on escalated frames.
    out->buffer[4] = (frame->buffer[4] & ~0xC0u) | 0x40u;
    out->buffer[5] = frame->buffer[5];

    // counter + 1 (byte6 lower nibble)
    uint8_t cnt = (frame->buffer[6] & 0x0F);
    cnt = (cnt + 1) & 0x0F;
    out->buffer[6] = (frame->buffer[6] & 0xF0) | cnt;

    // checksum: sum(byte0..6) + 0x70 + 0x03 (CAN ID 0x370 split)
    out->buffer[7] = tesla_additive_checksum(CAN_ID_EPAS_STATUS, out->buffer, 7);

    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}
