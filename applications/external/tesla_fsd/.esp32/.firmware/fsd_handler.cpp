/*
 * fsd_handler.cpp
 *
 * CAN frame manipulation logic for Tesla FSD unlock.
 * Ported from hypery11/flipper-tesla-fsd  fsd_logic/fsd_handler.c
 *
 * All bit operations, mux dispatch, speed profile mapping, and checksum
 * calculations are kept bit-for-bit identical to the upstream Flipper Zero
 * implementation.
 */

#include "fsd_handler.h"
#include "can_signals.h"
#include <string.h>

// ── Internal helpers ──────────────────────────────────────────────────────────

static void set_bit(CanFrame *frame, int bit, bool value) {
    if (bit < 0 || bit >= CAN_FRAME_MAX_BITS) return;
    int byte_idx = bit / 8;
    int bit_idx  = bit % 8;
    uint8_t mask = (uint8_t)(1U << bit_idx);
    if (value)
        frame->data[byte_idx] |= mask;
    else
        frame->data[byte_idx] &= (uint8_t)(~mask);
}

static uint8_t read_mux_id(const CanFrame *frame) {
    // MUX ID is the lower 3 bits of byte 0
    return frame->data[CAN_MUX_BYTE] & CAN_MUX_MASK;
}

static bool is_fsd_selected(const CanFrame *frame, bool force_fsd, bool china_mode) {
    if (force_fsd) return true;
    if (china_mode) return true;
    if (frame->dlc < 5) return false;
    // DAS_autopilotControl byte 4 bits [7:6] = UI "FSD selected" flag (bit 38 in the 64-bit data
    // field).  Note: bit 46 is the *output* FSD-activation bit written to the modified frame —
    // a different field at byte 5 bit 6.
    return (frame->data[SIG_AP_UI_FSD_SELECTED_BYTE] >> SIG_AP_UI_FSD_SELECTED_SHIFT) &
           SIG_AP_UI_FSD_SELECTED_MASK;
}

// ── State init ────────────────────────────────────────────────────────────────

void fsd_state_init(FSDState *state, TeslaHWVersion hw) {
    memset(state, 0, sizeof(FSDState));
    fsd_apply_hw_version(state, hw);
    state->op_mode    = OpMode_ListenOnly;  // safe default — never TX on boot

    // Feature flags: nag killer and chime suppress default ON; others OFF
    state->nag_killer           = true;
    state->suppress_speed_chime = true;
    state->ignore_ota           = false;
    state->emergency_vehicle_detect = false;
    state->force_fsd            = false;
    state->china_mode           = false;
    state->bms_output           = false;
#if defined(BOARD_TTGO_DISPLAY)
    state->display_enabled      = true;
    state->display_brightness   = 50;
    state->display_timeout_s    = 60;
#endif
    state->sleep_idle_ms        = SLEEP_IDLE_MS;

    strncpy(state->wifi_ssid, "Tesla-FSD", sizeof(state->wifi_ssid));
    strncpy(state->wifi_pass, "12345678",  sizeof(state->wifi_pass));
    state->wifi_hidden = false;
}

void fsd_apply_hw_version(FSDState *state, TeslaHWVersion hw) {
    state->hw_version = hw;
    // Default speed profile per HW version
    if (hw == TeslaHW_HW4)
        state->speed_profile = 4;
    else if (hw == TeslaHW_Legacy)
        state->speed_profile = 1;
    else
        state->speed_profile = 2;
}

// ── Transmit gate ─────────────────────────────────────────────────────────────

bool fsd_can_transmit(const FSDState *state) {
    if (state->op_mode == OpMode_ListenOnly) return false;
    if (state->tesla_ota_in_progress && !state->ignore_ota) return false;
    return true;
}

// ── HW version detection from GTW_carConfig (0x398) ──────────────────────────

TeslaHWVersion fsd_detect_hw_version(const CanFrame *frame) {
    if (frame->id != CAN_ID_GTW_CAR_CONFIG) return TeslaHW_Unknown;
    // DAS_HWversion field: bits 7:6 of byte 0  (das_hw)
    uint8_t das_hw = (frame->data[SIG_GTW_DAS_HW_BYTE] >> SIG_GTW_DAS_HW_SHIFT) &
                     SIG_GTW_DAS_HW_MASK;
    switch (das_hw) {
        case SIG_GTW_DAS_HW_LEGACY_0:
        case SIG_GTW_DAS_HW_LEGACY_1:
            return TeslaHW_Legacy;   // HW1/HW2/EAP retrofit — uses 0x3EE/0x045
        case SIG_GTW_DAS_HW_HW3: return TeslaHW_HW3;
        case SIG_GTW_DAS_HW_HW4: return TeslaHW_HW4;
        default: return TeslaHW_Unknown;
    }
}

// ── OTA detection from GTW_carState (0x318) ───────────────────────────────────

void fsd_handle_gtw_car_state(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 7) return;
    // GTW_updateInProgress: bits 1:0 of byte 6.
    // Filter transient / incompatible values to avoid false positives.
    uint8_t raw = frame->data[SIG_GTW_UPDATE_IN_PROGRESS_BYTE] &
                  SIG_GTW_UPDATE_IN_PROGRESS_MASK;
    state->ota_raw_state = raw;

    bool in_progress = (raw == OTA_IN_PROGRESS_RAW_VALUE);
    if (in_progress) {
        if (state->ota_assert_count < 255u) state->ota_assert_count++;
        state->ota_clear_count = 0;
        if (state->ota_assert_count >= OTA_ASSERT_FRAMES)
            state->tesla_ota_in_progress = true;
    } else {
        if (state->ota_clear_count < 255u) state->ota_clear_count++;
        state->ota_assert_count = 0;
        if (state->ota_clear_count >= OTA_CLEAR_FRAMES)
            state->tesla_ota_in_progress = false;
    }
}

// ── Follow distance → speed profile (DAS_followDistance 0x3F8) ───────────────

void fsd_handle_follow_distance(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 6) return;
    // Follow distance stalk position: bits 7:5 of byte 5
    uint8_t fd = (frame->data[SIG_FOLLOW_DIST_BYTE] & SIG_FOLLOW_DIST_MASK) >>
                 SIG_FOLLOW_DIST_SHIFT;

    if (state->hw_version == TeslaHW_HW3) {
        // HW3: 3 levels  (fd 1→profile 2, 2→1, 3→0)
        switch (fd) {
            case 1: state->speed_profile = 2; break;
            case 2: state->speed_profile = 1; break;
            case 3: state->speed_profile = 0; break;
            default: break;
        }
    } else {
        // HW4: 5 levels  (fd 1→3, 2→2, 3→1, 4→0, 5→4)
        switch (fd) {
            case 1: state->speed_profile = 3; break;
            case 2: state->speed_profile = 2; break;
            case 3: state->speed_profile = 1; break;
            case 4: state->speed_profile = 0; break;
            case 5: state->speed_profile = 4; break;
            default: break;
        }
    }
}

// ── HW3/HW4 autopilot control (DAS_autopilotControl 0x3FD) ───────────────────

bool fsd_handle_autopilot_frame(FSDState *state, CanFrame *frame) {
    if (frame->dlc < 8) return false;
    // Only process known HW versions to avoid corrupting frames for HW_Unknown
    if (state->hw_version != TeslaHW_HW3 && state->hw_version != TeslaHW_HW4)
        return false;

    uint8_t mux     = read_mux_id(frame);
    bool    fsd_ui  = is_fsd_selected(frame, state->force_fsd, state->china_mode);
    bool    modified = false;

    // mux 0 is the authoritative "is FSD requested" mux
    if (mux == CAN_MUX_0) state->fsd_enabled = fsd_ui;

    if (state->hw_version == TeslaHW_HW3) {
        // ── HW3 ──────────────────────────────────────────────────────────────
        if (mux == CAN_MUX_0 && state->fsd_enabled) {
            // Compute speed offset from current speed signal (bits 6:1 of byte 3)
            int raw = (int)((frame->data[SIG_AP_HW3_SPEED_RAW_BYTE] >>
                             SIG_AP_HW3_SPEED_RAW_SHIFT) &
                            SIG_AP_HW3_SPEED_RAW_MASK) - SIG_AP_HW3_SPEED_RAW_ZERO;
            int offset = raw * SIG_AP_HW3_SPEED_OFFSET_STEP;
            if (offset < SIG_AP_HW3_SPEED_OFFSET_MIN) offset = SIG_AP_HW3_SPEED_OFFSET_MIN;
            if (offset > SIG_AP_HW3_SPEED_OFFSET_MAX) offset = SIG_AP_HW3_SPEED_OFFSET_MAX;
            state->speed_offset = offset;

            // Activate FSD: set bit 46
            set_bit(frame, SIG_AP_FSD_ENABLE_BIT, true);

            // Write speed profile into bits 2:1 of byte 6
            frame->data[SIG_AP_SPEED_PROFILE_BYTE] &= (uint8_t)(~SIG_AP_SPEED_PROFILE_MASK);
            frame->data[SIG_AP_SPEED_PROFILE_BYTE] |=
                (uint8_t)((state->speed_profile & SIG_AP_SPEED_PROFILE_VALUE_MASK) <<
                          SIG_AP_SPEED_PROFILE_SHIFT);
            modified = true;
        }
        if (mux == CAN_MUX_1) {
            // Nag suppression via bit 19 (clear = no hands-on-wheel request)
            set_bit(frame, SIG_AP_NAG_CLEAR_BIT, false);
            state->nag_suppressed = true;
            modified = true;
        }
        if (mux == CAN_MUX_2 && state->fsd_enabled) {
            // Write speed offset into bits 7:6 of byte 0 and bits 5:0 of byte 1
            frame->data[SIG_AP_HW3_SPEED_OFFSET_LOW_BYTE] &=
                (uint8_t)(~SIG_AP_HW3_SPEED_OFFSET_LOW_MASK);
            frame->data[SIG_AP_HW3_SPEED_OFFSET_HIGH_BYTE] &=
                (uint8_t)(~SIG_AP_HW3_SPEED_OFFSET_HIGH_MASK);
            frame->data[SIG_AP_HW3_SPEED_OFFSET_LOW_BYTE] |=
                (uint8_t)((state->speed_offset & SIG_AP_HW3_SPEED_OFFSET_LOW_VALUE_MASK) <<
                          SIG_AP_HW3_SPEED_OFFSET_LOW_SHIFT);
            frame->data[SIG_AP_HW3_SPEED_OFFSET_HIGH_BYTE] |=
                (uint8_t)(state->speed_offset >> SIG_AP_HW3_SPEED_OFFSET_HIGH_SHIFT);
            modified = true;
        }
    } else {
        // ── HW4 ──────────────────────────────────────────────────────────────
        if (mux == CAN_MUX_0 && state->fsd_enabled) {
            set_bit(frame, SIG_AP_FSD_ENABLE_BIT, true);       // FSD activation
            set_bit(frame, SIG_AP_HW4_FSD_ENABLE_BIT, true);   // HW4 additional FSD bit
            if (state->emergency_vehicle_detect)
                set_bit(frame, SIG_AP_HW4_EMERGENCY_VEHICLE_BIT, true);
            modified = true;
        }
        if (mux == CAN_MUX_1) {
            set_bit(frame, SIG_AP_NAG_CLEAR_BIT, false);      // clear hands-on-wheel nag
            set_bit(frame, SIG_AP_HW4_NAG_CONFIRM_BIT, true); // HW4 nag-suppression confirmation bit
            state->nag_suppressed = true;
            modified = true;
        }
        if (mux == CAN_MUX_2) {
            // Write speed profile into bits 6:4 of byte 7
            frame->data[SIG_AP_HW4_SPEED_PROFILE_BYTE] &=
                (uint8_t)(~(SIG_AP_HW4_SPEED_PROFILE_MASK << SIG_AP_HW4_SPEED_PROFILE_SHIFT));
            frame->data[SIG_AP_HW4_SPEED_PROFILE_BYTE] |=
                (uint8_t)((state->speed_profile & SIG_AP_HW4_SPEED_PROFILE_MASK) <<
                          SIG_AP_HW4_SPEED_PROFILE_SHIFT);
            modified = true;
        }
    }

    if (modified) state->frames_modified++;
    return modified;
}

// ── Legacy autopilot (DAS_autopilot 0x3EE) ───────────────────────────────────

void fsd_handle_legacy_stalk(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 2) return;
    // STW_ACTN_RQ: stalk position encoded in bits 7:5 of byte 1
    // 0x00=Pos1, 0x21=Pos2, 0x42=Pos3, 0x64=Pos4, 0x85=Pos5, 0xA6=Pos6, 0xC8=Pos7
    uint8_t pos = frame->data[SIG_LEGACY_STALK_POS_BYTE] >> SIG_LEGACY_STALK_POS_SHIFT;
    if (pos <= 1)
        state->speed_profile = 2;
    else if (pos == 2)
        state->speed_profile = 1;
    else
        state->speed_profile = 0;
}

bool fsd_handle_legacy_autopilot(FSDState *state, CanFrame *frame) {
    if (frame->dlc < 8) return false;

    uint8_t mux    = read_mux_id(frame);
    bool    fsd_ui = is_fsd_selected(frame, state->force_fsd, state->china_mode);
    bool    modified = false;

    if (mux == CAN_MUX_0) state->fsd_enabled = fsd_ui;

    if (mux == CAN_MUX_0 && state->fsd_enabled) {
        set_bit(frame, SIG_AP_FSD_ENABLE_BIT, true);
        // Speed profile in bits 2:1 of byte 6 (same encoding as HW3)
        frame->data[SIG_AP_SPEED_PROFILE_BYTE] &= (uint8_t)(~SIG_AP_SPEED_PROFILE_MASK);
        frame->data[SIG_AP_SPEED_PROFILE_BYTE] |=
            (uint8_t)((state->speed_profile & SIG_AP_SPEED_PROFILE_VALUE_MASK) <<
                      SIG_AP_SPEED_PROFILE_SHIFT);
        modified = true;
    }
    if (mux == CAN_MUX_1) {
        set_bit(frame, SIG_AP_NAG_CLEAR_BIT, false);
        state->nag_suppressed = true;
        modified = true;
    }

    if (modified) state->frames_modified++;
    return modified;
}

// ── ISA speed chime suppress (0x399, HW4 only) ───────────────────────────────

bool fsd_handle_isa_speed_chime(CanFrame *frame) {
    if (frame->dlc < 8) return false;
    // Set "ISA_speedLimitSoundActive" flag: bit 5 of byte 1
    frame->data[SIG_ISA_SOUND_ACTIVE_BYTE] |= SIG_ISA_SOUND_ACTIVE_MASK;
    // Recalculate Tesla checksum: sum(byte0..6) + low(CAN_ID) + high(CAN_ID)
    // CAN_ID_ISA_SPEED = 0x399 → low=0x99, high=0x03
    uint8_t sum = 0;
    for (int i = 0; i < 7; i++)
        sum += frame->data[i];
    sum += (uint8_t)(CAN_ID_ISA_SPEED & 0xFFu) + (uint8_t)(CAN_ID_ISA_SPEED >> 8);
    frame->data[7] = sum;
    return true;
}

// ── NAG killer: counter+1 echo of EPAS3P_sysStatus (0x370) ──────────────────
//
// When handsOnLevel == 0 (nag imminent) or 3 (escalated alarm), we send a
// spoofed EPAS frame with handsOnLevel=1 and counter+1 before the real frame
// reaches the DAS.  The DAS sees "hands on" and drops the nag.
//
// DAS-aware gating: also checks das_hands_on_state from 0x39B.  States 0
// (NOT_REQD) and 8 (SUSPENDED) mean DAS is already satisfied — skip the echo
// to avoid ~25 spurious frames/sec on the bus.  If 0x39B has never been seen
// (das_seen==false), we echo conservatively based on EPAS level alone.
//
// Organic torque: torsionBarTorque uses a xorshift32 random walk [1.00–2.40 Nm]
// with brief grip pulses [3.10–3.30 Nm] every 5–9 s.  A flat signal for 30+
// minutes is a statistical impossibility from a real hand and is a known
// telemetry detection vector.
//
// Checksum: byte7 = (sum(byte0..6) + 0x70 + 0x03) & 0xFF  (CAN ID 0x370 split)

static uint32_t nag_prng_state       = 0xDEADBEEFu;
static int16_t  nag_torq_walk        = 2230;   // raw init ≈ 1.80 Nm
static uint8_t  nag_exc_frames       = 0;
static uint16_t nag_frames_until_exc = 175;

static uint32_t nag_xorshift32() {
    uint32_t x = nag_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    nag_prng_state = x;
    return x;
}

bool fsd_handle_nag_killer(FSDState *state, const CanFrame *frame, CanFrame *out) {
    if (frame->dlc < 8)     return false;
    if (!state->nag_killer) return false;

    // EPAS handsOnLevel: bits 7:6 of byte 4.  Skip only when level==1 (hands OK).
    uint8_t hands_on = (frame->data[SIG_EPAS_HANDS_ON_BYTE] >> SIG_EPAS_HANDS_ON_SHIFT) &
                       SIG_EPAS_HANDS_ON_MASK;
    if (hands_on == SIG_EPAS_HANDS_ON_OK) return false;

    // DAS-aware gating — skip echo when DAS itself is satisfied.
    if (state->das_seen) {
        uint8_t das = state->das_hands_on_state;
        if (das == SIG_DAS_HANDS_ON_NOT_REQUIRED || das == SIG_DAS_HANDS_ON_SUSPENDED)
            return false;
    }

    // Organic torque random walk
    int16_t torq;
    if (nag_exc_frames > 0) {
        // Grip pulse: ~3.20 Nm ± noise
        torq = 2350 + (int16_t)((int)(nag_xorshift32() % 41u) - 20);
        nag_exc_frames--;
    } else {
        int16_t step = (int16_t)((int)(nag_xorshift32() % 31u) - 15);
        nag_torq_walk += step;
        if (nag_torq_walk < 2150) nag_torq_walk = 2150;  // min ~1.00 Nm
        if (nag_torq_walk > 2290) nag_torq_walk = 2290;  // max ~2.40 Nm
        torq = nag_torq_walk;
        if (nag_frames_until_exc > 0) {
            nag_frames_until_exc--;
        } else {
            nag_exc_frames       = (uint8_t)(3u + (nag_xorshift32() % 3u));
            nag_frames_until_exc = (uint16_t)(125u + (nag_xorshift32() % 100u));
        }
    }

    out->id  = CAN_ID_EPAS_STATUS;
    out->dlc = 8;

    out->data[0] = frame->data[0];
    out->data[1] = frame->data[1];
    out->data[SIG_EPAS_TORQUE_HIGH_BYTE] =
        (frame->data[SIG_EPAS_TORQUE_HIGH_BYTE] & SIG_EPAS_TORQUE_HIGH_KEEP_MASK) |
        (uint8_t)((torq >> SIG_EPAS_TORQUE_HIGH_SHIFT) &
                  SIG_EPAS_TORQUE_HIGH_VALUE_MASK);
    out->data[SIG_EPAS_TORQUE_LOW_BYTE] = (uint8_t)(torq & SIG_EPAS_TORQUE_LOW_MASK);
    out->data[SIG_EPAS_HANDS_ON_BYTE] =
        (frame->data[SIG_EPAS_HANDS_ON_BYTE] & (uint8_t)(~SIG_EPAS_HANDS_ON_CLEAR_MASK)) |
        SIG_EPAS_HANDS_ON_SPOOF_VALUE;  // handsOnLevel = 1
    out->data[5] = frame->data[5];

    // counter+1: lower nibble of byte 6
    uint8_t cnt = (frame->data[SIG_EPAS_COUNTER_BYTE] & SIG_EPAS_COUNTER_MASK);
    cnt = (cnt + 1u) & SIG_EPAS_COUNTER_MASK;
    out->data[SIG_EPAS_COUNTER_BYTE] =
        (frame->data[SIG_EPAS_COUNTER_BYTE] & SIG_EPAS_COUNTER_KEEP_MASK) | cnt;

    // Checksum
    uint16_t sum = 0;
    for (int i = 0; i < 7; i++)
        sum += out->data[i];
    sum += (CAN_ID_EPAS_STATUS & 0xFFu) + (CAN_ID_EPAS_STATUS >> 8);
    out->data[7] = (uint8_t)(sum & 0xFFu);

    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}

// ── BMS read-only parsers ─────────────────────────────────────────────────────

void fsd_handle_bms_hv(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 4) return;
    // Voltage: uint16 little-endian bytes 1:0, LSB = 0.01 V
    uint16_t raw_v = ((uint16_t)frame->data[SIG_BMS_VOLTAGE_H_BYTE] << 8) |
                     frame->data[SIG_BMS_VOLTAGE_L_BYTE];
    // Current: int16 little-endian bytes 3:2, LSB = 0.1 A (signed)
    int16_t raw_i = (int16_t)(((uint16_t)frame->data[SIG_BMS_CURRENT_H_BYTE] << 8) |
                              frame->data[SIG_BMS_CURRENT_L_BYTE]);
    state->pack_voltage_v = raw_v * SIG_BMS_VOLTAGE_SCALE;
    state->pack_current_a = raw_i * SIG_BMS_CURRENT_SCALE;
    state->bms_seen = true;
}

void fsd_handle_bms_soc(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 2) return;
    // SoC: 10-bit little-endian (bits 9:0 across bytes 1:0), LSB = 0.1 %
    uint16_t raw = ((uint16_t)(frame->data[SIG_BMS_SOC_H_BYTE] & SIG_BMS_SOC_H_MASK) << 8) |
                   frame->data[SIG_BMS_SOC_L_BYTE];
    state->soc_percent = raw * SIG_BMS_SOC_SCALE;
    state->bms_seen = true;
}

void fsd_handle_bms_thermal(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 6) return;
    // Temperatures: raw byte − 40 = °C
    state->batt_temp_min_c = (int8_t)((int)frame->data[SIG_BMS_TEMP_MIN_BYTE] -
                                      SIG_BMS_TEMP_OFFSET);
    state->batt_temp_max_c = (int8_t)((int)frame->data[SIG_BMS_TEMP_MAX_BYTE] -
                                      SIG_BMS_TEMP_OFFSET);
    state->bms_seen = true;
}

// ── Precondition trigger ──────────────────────────────────────────────────────

void fsd_build_precondition_frame(CanFrame *frame) {
    memset(frame, 0, sizeof(CanFrame));
    frame->id  = CAN_ID_TRIP_PLANNING;
    frame->dlc = 8;
    // byte0: bit0 = tripPlanningActive, bit2 = requestActiveBatteryHeating
    frame->data[SIG_TRIP_PLANNING_FLAGS_BYTE] = SIG_TRIP_PLANNING_PRECONDITION;
}

// ── TLSSC Restore (0x331) ─────────────────────────────────────────────────────

bool fsd_handle_tlssc_restore(FSDState *state, CanFrame *frame) {
    if (!state->tlssc_restore) return false;
    if (frame->dlc < 1) return false;

    uint8_t original = frame->data[SIG_DAS_AP_CONFIG_TIER_BYTE];
    uint8_t modified = (original & SIG_DAS_AP_CONFIG_KEEP_MASK) |
                       SIG_DAS_AP_CONFIG_SELF_DRIVING;

    if (modified == original) return false;

    frame->data[SIG_DAS_AP_CONFIG_TIER_BYTE] = modified;
    state->tlssc_restore_count++;
    return true;
}

static void fsd_handle_das_status_common(FSDState *state, const CanFrame *frame) {
    if (frame->dlc != CAN_FRAME_MAX_DATA_LEN) return;

    state->das_speed_limit_1 = frame->data[SIG_DAS_SPEED_LIMIT_BYTE_1];
    state->das_speed_limit_2 = frame->data[SIG_DAS_SPEED_LIMIT_BYTE_2];

    // DAS_autopilotHandsOnState: byte5 bits[5:2].
    state->das_hands_on_state =
        (frame->data[SIG_DAS_HANDS_ON_STATE_BYTE] >> SIG_DAS_HANDS_ON_STATE_SHIFT) &
        SIG_DAS_HANDS_ON_STATE_MASK;
    state->das_lane_change_state = frame->data[SIG_DAS_LANE_CHANGE_STATE_BYTE];
    state->das_counter = frame->data[SIG_DAS_COUNTER_BYTE];
    state->das_checksum = frame->data[SIG_DAS_CHECKSUM_BYTE];
    state->das_seen = true;
}

// ── DAS status — nag killer gating / AP active status ────────────────────────

void fsd_handle_das_status_hw3(FSDState *state, const CanFrame *frame) {
    if (frame->dlc != CAN_FRAME_MAX_DATA_LEN) return;

    // Legacy/HW3 0x399 layout: DAS_autopilotState is byte0 low nibble.
    // Observed HW3 mapping: 2=available/ready, 3=engaged.
    state->das_ap_state =
        frame->data[SIG_DAS_HW3_AP_STATE_BYTE] & SIG_DAS_HW3_AP_STATE_MASK;
    state->ap_active = state->das_ap_state == SIG_DAS_HW3_AP_ACTIVE_STATE;
    fsd_handle_das_status_common(state, frame);
}

void fsd_handle_das_status_hw4(FSDState *state, const CanFrame *frame) {
    if (frame->dlc != CAN_FRAME_MAX_DATA_LEN) return;

    // HW4 0x39B layout from the original parser: bit12|4 = byte1 bits[7:4].
    state->das_ap_state =
        (frame->data[SIG_DAS_HW4_AP_STATE_BYTE] >> SIG_DAS_HW4_AP_STATE_SHIFT) &
        SIG_DAS_HW4_AP_STATE_MASK;
    state->ap_active = state->das_ap_state >= SIG_DAS_HW4_AP_ACTIVE_MIN;
    fsd_handle_das_status_common(state, frame);
}
