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
#include "../../fsd_logic/fsd_checksum.h"  // shared Tesla additive checksum (single impl, both platforms)
#include "../../fsd_logic/fsd_can_ops.h"   // shared stateless frame primitives (set_bit / mux / fsd-selected)
#include <string.h>

// ── Internal helpers ──────────────────────────────────────────────────────────

// DAS_autopilotControl byte 4 bits [7:6] = UI "FSD selected" flag (bit 38 in the 64-bit data
// field). Note: bit 46 is the *output* FSD-activation bit written to the modified frame —
// a different field at byte 5 bit 6. Logic shared with the Flipper via fsd_can_ops.h.
static void set_bit(CanFrame *frame, int bit, bool value) {
    tesla_set_bit(frame->data, bit, value);
}

static uint8_t read_mux_id(const CanFrame *frame) {
    return tesla_read_mux(frame->data);
}

static bool is_fsd_selected(const CanFrame *frame, bool force_fsd, bool china_mode) {
    return tesla_is_fsd_selected(frame->data, frame->dlc, force_fsd, china_mode);
}

// ── State init ────────────────────────────────────────────────────────────────

void fsd_state_init(FSDState *state, TeslaHWVersion hw) {
    memset(state, 0, sizeof(FSDState));
    fsd_apply_hw_version(state, hw);
    state->das_prev_hands_on_state = 0xFF;  // nag escalation-edge baseline (#100)
    state->op_mode    = OpMode_ListenOnly;  // safe default — never TX on boot

    // Feature flags: nag killer and chime suppress default ON; others OFF
    state->nag_killer           = true;
    state->continuous_ap        = false;
    state->suppress_speed_chime = true;
    state->ignore_ota           = false;
    state->emergency_vehicle_detect = false;
    state->fsd_unlock           = false;
    state->force_fsd            = false;
    state->china_mode           = false;
    state->bms_output           = false;
    // 14.x warning default ON — most affected users don't know their firmware version
    state->firmware_14x_warning = true;
#if defined(BOARD_TTGO_DISPLAY)
    state->display_enabled      = true;
    state->display_brightness   = 50;
    state->display_timeout_s    = 60;
#endif
    state->sleep_idle_ms        = SLEEP_IDLE_MS;

    strncpy(state->wifi_ssid, "Tesla-FSD", sizeof(state->wifi_ssid));
    strncpy(state->wifi_pass, "12345678",  sizeof(state->wifi_pass));
    state->wifi_hidden = false;
    state->wifi_sta_ssid[0] = '\0';
    state->wifi_sta_pass[0] = '\0';
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

// AP-First gate (parity with the Flipper). When ap_first is on, hold AP/FSD/nag
// injection until AP is engaged (das_ap_state >= 2) AND has held stable for
// AP_FIRST_STABLE_MS — injecting on the activation edge is what trips the
// 2026.14.x preflight (steer-jerk + AP disengage). now_ms = millis(),
// ap_unstable_tick_ms is stamped whenever das_ap_state < 2.
bool fsd_ap_first_allows(const FSDState *state, uint32_t now_ms) {
    if (!state->ap_first) return true;             // gate off -> always allow
    if (state->das_ap_state < 2u) return false;    // AP not engaged yet
    return (now_ms - state->ap_unstable_tick_ms) >= AP_FIRST_STABLE_MS;
}

// ── HW version detection from GTW_carConfig (0x398) ──────────────────────────

TeslaHWVersion fsd_detect_hw_version(const CanFrame *frame) {
    if (frame->id != CAN_ID_GTW_CAR_CONFIG) return TeslaHW_Unknown;
    // Some HW4 trims (Juniper/Giga) forward an all-zero 0x398 stub on the
    // gateway copy that reaches Bus 6 — only the mux byte ever moves. An empty
    // GTW_carConfig carries no real das_hw; reading it as 0 would mislabel the
    // car as Legacy and route it through the 0x3EE path. The real HW4 marker
    // lives on Vehicle CAN (0x39B). Treat an all-zero payload as Unknown so
    // detection falls through to the live markers instead of a stub.
    bool all_zero = true;
    for (uint8_t i = 0; i < frame->dlc && i < CAN_FRAME_MAX_DATA_LEN; i++) {
        if (frame->data[i] != 0u) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) return TeslaHW_Unknown;
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
        if (mux == CAN_MUX_0 && state->fsd_unlock && state->fsd_enabled) {
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
        if (mux == CAN_MUX_1 && state->nag_killer) {
            // Nag suppression via bit 19 (clear = no hands-on-wheel request)
            set_bit(frame, SIG_AP_NAG_CLEAR_BIT, false);
            state->nag_suppressed = true;
            modified = true;
        }
        if (mux == CAN_MUX_2 && state->fsd_unlock && state->fsd_enabled) {
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
        if (mux == CAN_MUX_0 && state->fsd_unlock && state->fsd_enabled) {
            set_bit(frame, SIG_AP_FSD_ENABLE_BIT, true);       // FSD activation
            set_bit(frame, SIG_AP_HW4_FSD_ENABLE_BIT, true);   // HW4 additional FSD bit
            if (state->emergency_vehicle_detect)
                set_bit(frame, SIG_AP_HW4_EMERGENCY_VEHICLE_BIT, true);
            modified = true;
        }
        if (mux == CAN_MUX_1 && state->nag_killer) {
            set_bit(frame, SIG_AP_NAG_CLEAR_BIT, false);      // clear hands-on-wheel nag
            set_bit(frame, SIG_AP_HW4_NAG_CONFIRM_BIT, true); // HW4 nag-suppression confirmation bit
            state->nag_suppressed = true;
            modified = true;
        }
        if (mux == CAN_MUX_2 && state->fsd_unlock) {
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

    if (mux == CAN_MUX_0 && state->fsd_unlock && state->fsd_enabled) {
        set_bit(frame, SIG_AP_FSD_ENABLE_BIT, true);
        // Speed profile in bits 2:1 of byte 6 (same encoding as HW3)
        frame->data[SIG_AP_SPEED_PROFILE_BYTE] &= (uint8_t)(~SIG_AP_SPEED_PROFILE_MASK);
        frame->data[SIG_AP_SPEED_PROFILE_BYTE] |=
            (uint8_t)((state->speed_profile & SIG_AP_SPEED_PROFILE_VALUE_MASK) <<
                      SIG_AP_SPEED_PROFILE_SHIFT);
        modified = true;
    }
    if (mux == CAN_MUX_1 && state->nag_killer) {
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
    // Recalculate Tesla checksum (shared impl): sum(byte0..6) + low(CAN_ID) + high(CAN_ID)
    frame->data[7] = tesla_additive_checksum(CAN_ID_ISA_SPEED, frame->data, 7);
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

// EPAS-faithful (Mode-C) path — mirror of the Flipper fsd_handler.c logic (#100):
// demand-state-driven torque, handsOnLevel derived from torque magnitude, direction
// opposite steering. Raw 2048 = 0 Nm, raw = 2048 + Nm*100.
static uint8_t nag_hands_level_from_raw(int16_t raw) {
    int16_t a = (raw >= 2048) ? (raw - 2048) : (2048 - raw);
    if (a >= 200) return 2;
    if (a >= 100) return 1;
    return 0;
}

static bool nag_faithful_modec(FSDState *state, const CanFrame *frame,
                               CanFrame *out, uint32_t now_ms) {
    uint8_t das = state->das_hands_on_state;

    static uint8_t  prev_das        = 0xFF;
    static uint32_t s1_enter_ms     = 0;
    static uint32_t s2_enter_ms     = 0;
    static uint32_t strong_enter_ms = 0;
    static int16_t  last_raw        = 2048;
    static uint8_t  last_level      = 0;
    static uint32_t s2_hold_until_ms = 0;
    static int16_t  s2_hold_raw     = 2048;
    static uint8_t  s2_hold_level   = 0;
    static bool     s2_level2_active = false;
    static int16_t  mild_walk       = 2048;

    bool is_strong   = (das == 3 || das == 4 || das == 5);
    bool prev_strong = (prev_das == 3 || prev_das == 4 || prev_das == 5);
    if (prev_das != 1 && das == 1) s1_enter_ms = now_ms;
    if (das != 1) s1_enter_ms = 0;
    if (prev_das != 2 && das == 2) s2_enter_ms = now_ms;
    if (das != 2) { s2_enter_ms = 0; s2_hold_until_ms = 0; s2_level2_active = false; }
    if (!prev_strong && is_strong) strong_enter_ms = now_ms;
    if (!is_strong) strong_enter_ms = 0;
    prev_das = das;

    if (state->das_ap_state < 2u || das == 0 || das == 8 || das == 15) {
        last_raw = 2048; last_level = 0;
        return false;
    }

    int dir = (state->steering_angle_deg > 0.0f) ? -1 : 1;
    int16_t torque;
    uint8_t level;

    if (das == 1) {
        if (s1_enter_ms != 0 && (now_ms - s1_enter_ms) < 500u) {
            torque = last_raw; level = last_level;
        } else {
            last_raw = 2048; last_level = 0;
            return false;
        }
    } else if (das == 2) {
        if (s2_enter_ms != 0 && (now_ms - s2_enter_ms) < 2000u) return false;
        if (now_ms < s2_hold_until_ms) {
            torque = s2_hold_raw; level = s2_hold_level;
        } else {
            int16_t lo = (dir < 0) ? 1848 : 2098;
            int16_t hi = (dir < 0) ? 1998 : 2248;
            if (mild_walk < lo || mild_walk > hi) mild_walk = (int16_t)((lo + hi) / 2);
            mild_walk += (int16_t)((int)(nag_xorshift32() % 25u) - 12);
            if (mild_walk < lo) mild_walk = lo;
            if (mild_walk > hi) mild_walk = hi;
            torque = mild_walk;
            level = nag_hands_level_from_raw(torque);
            int16_t a = (torque >= 2048) ? (torque - 2048) : (2048 - torque);
            bool l2 = (a >= 200);
            if (l2 && !s2_level2_active) {
                s2_hold_until_ms = now_ms + 1000u; s2_hold_raw = torque; s2_hold_level = 2;
            }
            s2_level2_active = l2;
        }
    } else {
        if (strong_enter_ms != 0 && (now_ms - strong_enter_ms) < 1000u) return false;
        uint32_t active_ms = (strong_enter_ms == 0) ? 0u : (now_ms - strong_enter_ms - 1000u);
        uint16_t phase = (uint16_t)(active_ms % 1500u);
        int16_t mag = 210;
        if (phase < 500u) mag = (int16_t)(((int32_t)phase * 210) / 500);
        torque = (int16_t)(2048 + dir * mag);
        level = nag_hands_level_from_raw(torque);
    }

    last_raw = torque; last_level = level;

    out->id  = CAN_ID_EPAS_STATUS;
    out->dlc = 8;
    out->data[0] = frame->data[0];
    out->data[1] = frame->data[1];
    out->data[SIG_EPAS_TORQUE_HIGH_BYTE] =
        (frame->data[SIG_EPAS_TORQUE_HIGH_BYTE] & SIG_EPAS_TORQUE_HIGH_KEEP_MASK) |
        (uint8_t)((torque >> SIG_EPAS_TORQUE_HIGH_SHIFT) & SIG_EPAS_TORQUE_HIGH_VALUE_MASK);
    out->data[SIG_EPAS_TORQUE_LOW_BYTE] = (uint8_t)(torque & SIG_EPAS_TORQUE_LOW_MASK);
    out->data[SIG_EPAS_HANDS_ON_BYTE] =
        (frame->data[SIG_EPAS_HANDS_ON_BYTE] & (uint8_t)(~SIG_EPAS_HANDS_ON_CLEAR_MASK)) |
        (uint8_t)((level & SIG_EPAS_HANDS_ON_MASK) << SIG_EPAS_HANDS_ON_SHIFT);
    out->data[5] = frame->data[5];
    uint8_t cnt = (frame->data[SIG_EPAS_COUNTER_BYTE] & SIG_EPAS_COUNTER_MASK);
    cnt = (cnt + 1u) & SIG_EPAS_COUNTER_MASK;
    out->data[SIG_EPAS_COUNTER_BYTE] =
        (frame->data[SIG_EPAS_COUNTER_BYTE] & SIG_EPAS_COUNTER_KEEP_MASK) | cnt;
    out->data[7] = tesla_additive_checksum(CAN_ID_EPAS_STATUS, out->data, 7);
    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}

bool fsd_handle_nag_killer(FSDState *state, const CanFrame *frame, CanFrame *out,
                           uint32_t now_ms) {
    if (frame->dlc < 8)     return false;
    if (!state->nag_killer) return false;

    if (state->nag_epas_faithful) return nag_faithful_modec(state, frame, out, now_ms);

    // EPAS handsOnLevel: bits 7:6 of byte 4.  Skip only when level==1 (hands OK).
    uint8_t hands_on = (frame->data[SIG_EPAS_HANDS_ON_BYTE] >> SIG_EPAS_HANDS_ON_SHIFT) &
                       SIG_EPAS_HANDS_ON_MASK;
    if (hands_on == SIG_EPAS_HANDS_ON_OK) return false;

    // DAS-aware gating + escalation-edge tracking. Track das every frame so a
    // satisfied (0) wave resets the baseline for the next 0->2 rising edge (#100).
    uint8_t das = state->das_hands_on_state;
    uint8_t das_prev = state->das_prev_hands_on_state;
    state->das_prev_hands_on_state = das;
    if (state->das_seen &&
        (das == SIG_DAS_HANDS_ON_NOT_REQUIRED || das == SIG_DAS_HANDS_ON_SUSPENDED))
        return false;

    // On-demand grip pulse — fire a strong excursion when a nag demand appears,
    // via EPAS handsOnLevel (0=imminent, 3=escalated) OR a DAS escalation edge.
    // On some HW4 trims (#100) EPAS byte4 is frozen at 0, so the only signal that
    // moves is das stepping 0->2->3; the DAS edge re-arms the pulse on each wave.
    // Mirrors the Flipper fsd_handle_nag_killer logic. Source: #100. The ESP32
    // path previously had no on-demand pulse at all (only the periodic walk).
    bool das_escalation = state->das_seen && (das_prev == 0xFFu || das > das_prev);
    bool demand_now = (hands_on == 0u || hands_on == 3u);
    if ((das_escalation || (demand_now && !state->nag_demand_active)) && nag_exc_frames == 0) {
        nag_exc_frames       = (uint8_t)(3u + (nag_xorshift32() % 3u));
        nag_frames_until_exc = (uint16_t)(125u + (nag_xorshift32() % 100u));
    }
    state->nag_demand_active = demand_now;

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

    // Checksum (shared impl)
    out->data[7] = tesla_additive_checksum(CAN_ID_EPAS_STATUS, out->data, 7);

    state->nag_echo_count++;
    state->nag_suppressed = true;
    return true;
}

void fsd_handle_epas_status(FSDState *state, const CanFrame *frame) {
    if (frame->dlc <= SIG_EPAS_TORQUE_LOW_BYTE) return;

    uint16_t raw_torque =
        ((uint16_t)(frame->data[SIG_EPAS_TORQUE_HIGH_BYTE] &
                    SIG_EPAS_TORQUE_HIGH_VALUE_MASK) << SIG_EPAS_TORQUE_HIGH_SHIFT) |
        (uint16_t)(frame->data[SIG_EPAS_TORQUE_LOW_BYTE] & SIG_EPAS_TORQUE_LOW_MASK);

    state->torsion_bar_torque_nm =
        (float)raw_torque * SIG_EPAS_TORQUE_SCALE_NM + SIG_EPAS_TORQUE_OFFSET_NM;
    state->torsion_bar_torque_seen = true;
}

void fsd_handle_esp_status(FSDState *state, const CanFrame *frame) {
    if (frame->dlc <= SIG_ESP_DRIVER_BRAKE_BYTE) return;
    uint8_t brake =
        (frame->data[SIG_ESP_DRIVER_BRAKE_BYTE] >> SIG_ESP_DRIVER_BRAKE_SHIFT) &
        SIG_ESP_DRIVER_BRAKE_MASK;
    state->driver_brake_applied = brake >= SIG_ESP_DRIVER_BRAKE_APPLYING;
    state->brake_status_seen = true;
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
    if (frame->dlc < 3) return;
    // Car display SOC: SOCUI292, bit10|10, LSB = 0.1 %.
    uint16_t raw =
        (((uint16_t)frame->data[SIG_BMS_SOC_UI_HIGH_BYTE] << (8 - SIG_BMS_SOC_UI_LOW_SHIFT)) |
         (frame->data[SIG_BMS_SOC_UI_LOW_BYTE] >> SIG_BMS_SOC_UI_LOW_SHIFT)) &
        SIG_BMS_SOC_UI_MASK;
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
    uint8_t vision_raw =
        frame->data[SIG_DAS_VISION_SPEED_LIMIT_BYTE] & SIG_DAS_VISION_SPEED_LIMIT_MASK;
    if (vision_raw != 0u && vision_raw != SIG_DAS_VISION_SPEED_LIMIT_NONE) {
        state->vision_speed_limit_kph =
            (float)vision_raw * SIG_DAS_VISION_SPEED_LIMIT_SCALE_KPH;
        state->speed_limit_kph = state->vision_speed_limit_kph;
        state->speed_limit_source = SpeedLimitSource_Vision;
        state->speed_limit_seen = true;
    }

    // DAS_autopilotHandsOnState: byte5 bits[5:2].
    state->das_hands_on_state =
        (frame->data[SIG_DAS_HANDS_ON_STATE_BYTE] >> SIG_DAS_HANDS_ON_STATE_SHIFT) &
        SIG_DAS_HANDS_ON_STATE_MASK;
    state->das_lane_change_state = frame->data[SIG_DAS_LANE_CHANGE_STATE_BYTE];
    state->das_counter = frame->data[SIG_DAS_COUNTER_BYTE];
    state->das_checksum = frame->data[SIG_DAS_CHECKSUM_BYTE];
    state->das_seen = true;
    state->ap_ready = state->das_ap_state == 2u;
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

    // Standard HW4 0x39B layout: DAS_autopilotState = byte1 bits[7:4].
    uint8_t hw4_state =
        (frame->data[SIG_DAS_HW4_AP_STATE_BYTE] >> SIG_DAS_HW4_AP_STATE_SHIFT) &
        SIG_DAS_HW4_AP_STATE_MASK;
    // HW4 Highland (China MIC, fw 2026.20) instead carries DAS_autopilotState in
    // byte0 low nibble (HW3 position: 1=available, 2=ready, 3=engaged) while
    // byte1[7:4] is pinned at 1 the whole drive (#116).
    uint8_t b0_state =
        frame->data[SIG_DAS_HW3_AP_STATE_BYTE] & SIG_DAS_HW3_AP_STATE_MASK;

    // Auto-fallback detection (self-healing; re-evaluated each power cycle). The
    // unique Highland signature is byte0 reporting an active state (>=2) while
    // byte1[7:4] stays exactly 1: a standard HW4 car can't produce this durably
    // because its byte1[7:4] moves to >=2 the instant AP engages, which trips the
    // sticky disqualifier below and pins it to the standard byte1 reading for good.
    if (hw4_state != 1u) {
        state->das_hw4_byte1_moved = true;
        state->das_hw4_byte0_pin_count = 0;
    } else if (!state->das_hw4_use_byte0 && !state->das_hw4_byte1_moved &&
               b0_state >= SIG_DAS_HW4_AP_ACTIVE_MIN) {
        if (state->das_hw4_byte0_pin_count < SIG_DAS_HW4_BYTE0_PIN_LATCH)
            state->das_hw4_byte0_pin_count++;
        if (state->das_hw4_byte0_pin_count >= SIG_DAS_HW4_BYTE0_PIN_LATCH)
            state->das_hw4_use_byte0 = true;
    }

    if (state->das_hw4_use_byte0) {
        state->das_ap_state = b0_state;
        state->ap_active    = b0_state == SIG_DAS_HW3_AP_ACTIVE_STATE; // 3 = engaged
    } else {
        state->das_ap_state = hw4_state;
        state->ap_active    = hw4_state >= SIG_DAS_HW4_AP_ACTIVE_MIN;
    }
    fsd_handle_das_status_common(state, frame);
    state->das_hw4_status_seen = true;
}

// HW4 0x399 hands-on fallback — for HW4 trims that never broadcast 0x39B
// (observed on a Juniper RWD, Bus 6, #100). 0x399 carries the hands-on field in
// the same byte5[5:2] slot (verified against a captured nag run: 1→2→3 as the
// visual nag escalates). Reads ONLY that field — not das_ap_state — because the
// HW4 0x399 byte0 layout is unconfirmed (0x399 is the ISA chime there). Call
// only when das_hw4_status_seen is false.
void fsd_handle_das_handsonly_399(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 6) return;
    state->das_hands_on_state =
        (frame->data[SIG_DAS_HANDS_ON_STATE_BYTE] >> SIG_DAS_HANDS_ON_STATE_SHIFT) &
        SIG_DAS_HANDS_ON_STATE_MASK;
    state->das_seen = true;
}

static bool action_pulse_due(uint32_t now_ms, uint32_t last_ms) {
    return last_ms == 0u || (uint32_t)(now_ms - last_ms) > 300u;
}

static bool gear_pos_is_up(uint8_t pos) {
    return pos == SIG_GEAR_LEVER_HALF_UP || pos == SIG_GEAR_LEVER_FULL_UP;
}

static bool gear_pos_is_down(uint8_t pos) {
    return pos == SIG_GEAR_LEVER_HALF_DOWN || pos == SIG_GEAR_LEVER_FULL_DOWN;
}

static bool gear_same_direction(uint8_t a, uint8_t b) {
    return (gear_pos_is_up(a) && gear_pos_is_up(b)) ||
           (gear_pos_is_down(a) && gear_pos_is_down(b));
}

static uint8_t gear_stronger_pos(uint8_t current, uint8_t next) {
    if (current == SIG_GEAR_LEVER_CENTER) return next;
    if (!gear_same_direction(current, next)) return next;
    if (next == SIG_GEAR_LEVER_FULL_UP || next == SIG_GEAR_LEVER_FULL_DOWN) return next;
    return current;
}

static void emit_gear_lever_action(FSDState *state, uint8_t pos, uint32_t now_ms) {
    if (pos == SIG_GEAR_LEVER_FULL_UP &&
        action_pulse_due(now_ms, state->stalk_full_up_ms)) {
        state->stalk_full_up_ms = now_ms;
    }
}

void fsd_handle_gear_lever(FSDState *state, const CanFrame *frame, uint32_t now_ms) {
    static bool active = false;
    static uint8_t best_pos = SIG_GEAR_LEVER_CENTER;
    static uint32_t last_seen_ms = 0;

    if (frame->dlc < 2) return;

    uint8_t pos =
        (frame->data[SIG_GEAR_LEVER_POS_BYTE] >> SIG_GEAR_LEVER_POS_SHIFT) &
        SIG_GEAR_LEVER_POS_MASK;

    if (pos > SIG_GEAR_LEVER_FULL_DOWN) return;

    if (active && last_seen_ms != 0u && (uint32_t)(now_ms - last_seen_ms) > 500u) {
        emit_gear_lever_action(state, best_pos, last_seen_ms);
        active = false;
        best_pos = SIG_GEAR_LEVER_CENTER;
    }

    if (pos == SIG_GEAR_LEVER_CENTER) {
        if (active) {
            emit_gear_lever_action(state, best_pos, now_ms);
            active = false;
            best_pos = SIG_GEAR_LEVER_CENTER;
        }
        return;
    }

    if (!active || !gear_same_direction(best_pos, pos)) {
        if (active) emit_gear_lever_action(state, best_pos, last_seen_ms);
        active = true;
        best_pos = pos;
    } else {
        best_pos = gear_stronger_pos(best_pos, pos);
    }
    last_seen_ms = now_ms;
}

void fsd_handle_ui_map_data(FSDState *state, const CanFrame *frame, uint32_t now_ms) {
    if (frame->dlc < 2) return;
    uint8_t raw =
        frame->data[SIG_UI_MAP_SPEED_LIMIT_BYTE] & SIG_UI_MAP_SPEED_LIMIT_MASK;
    if (raw == SIG_UI_MAP_SPEED_LIMIT_UNKNOWN ||
        raw == SIG_UI_MAP_SPEED_LIMIT_UNLIMITED ||
        raw == SIG_UI_MAP_SPEED_LIMIT_SNA) {
        return;
    }

    if (raw == 1u) {
        state->map_speed_limit_kph = 5.0f;
    } else if (raw == SIG_UI_MAP_SPEED_LIMIT_7_KPH) {
        state->map_speed_limit_kph = 7.0f;
    } else {
        state->map_speed_limit_kph = (float)(raw - 1u) * 5.0f;
    }
    state->speed_limit_kph = state->map_speed_limit_kph;
    state->speed_limit_source = SpeedLimitSource_Map;
    state->speed_limit_seen = true;
    state->speed_limit_last_ms = now_ms;
}

void fsd_handle_das_status2(FSDState *state, const CanFrame *frame, uint32_t now_ms) {
    if (frame->dlc < 2) return;
    uint16_t raw =
        ((uint16_t)(frame->data[SIG_DAS_ACC_SPEED_LIMIT_HIGH_BYTE] &
                    SIG_DAS_ACC_SPEED_LIMIT_HIGH_MASK) << 8) |
        frame->data[SIG_DAS_ACC_SPEED_LIMIT_LOW_BYTE];
    if (raw == SIG_DAS_ACC_SPEED_LIMIT_NONE || raw == SIG_DAS_ACC_SPEED_LIMIT_SNA) return;

    float kph = (float)raw * SIG_DAS_ACC_SPEED_LIMIT_SCALE_MPH * MPH_TO_KPH;
    state->acc_speed_limit_kph = kph;
    if (!state->speed_limit_seen || state->speed_limit_source == SpeedLimitSource_None) {
        state->speed_limit_kph = kph;
        state->speed_limit_source = SpeedLimitSource_Acc;
        state->speed_limit_seen = true;
        state->speed_limit_last_ms = now_ms;
    }
}

void fsd_handle_das_control(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 2) return;
    uint16_t raw =
        ((uint16_t)(frame->data[SIG_DAS_CONTROL_SET_SPEED_HIGH_BYTE] &
                    SIG_DAS_CONTROL_SET_SPEED_HIGH_MASK) << 8) |
        frame->data[SIG_DAS_CONTROL_SET_SPEED_LOW_BYTE];
    if (raw == SIG_DAS_CONTROL_SET_SPEED_SNA) return;

    state->cruise_set_speed_kph = (float)raw * SIG_DAS_CONTROL_SET_SPEED_SCALE_KPH;
    state->cruise_set_speed_seen = true;
}

void fsd_handle_vcfront_lighting(FSDState *state, const CanFrame *frame) {
    if (frame->dlc < 1) return;
    uint8_t left_request =
        (frame->data[0] >> SIG_VCFRONT_INDICATOR_LEFT_SHIFT) & SIG_VCFRONT_INDICATOR_MASK;
    uint8_t right_request =
        (frame->data[0] >> SIG_VCFRONT_INDICATOR_RIGHT_SHIFT) & SIG_VCFRONT_INDICATOR_MASK;

    state->left_turn_active = left_request != SIG_VCFRONT_INDICATOR_OFF;
    state->right_turn_active = right_request != SIG_VCFRONT_INDICATOR_OFF;
    state->left_turn_status_seen = true;
    state->right_turn_status_seen = true;
    state->turn_status_seen = true;
}

bool fsd_build_gear_lever_frame(CanFrame *frame, uint8_t gear_pos, uint8_t counter) {
    static const uint8_t NEUTRAL_CRC_BY_COUNTER[16] = {
        0x46u, 0x44u, 0x52u, 0x6Du, 0x43u, 0x41u, 0xDDu, 0xF9u,
        0x4Cu, 0xA5u, 0xF6u, 0x8Cu, 0x49u, 0x2Fu, 0x31u, 0x3Bu,
    };
    static const uint8_t POSITION_CRC_XOR[5] = {
        0x00u, // center
        0xE0u, // half up
        0xEFu, // full up
        0x0Fu, // half down
        0xF1u, // full down
    };

    if (gear_pos > SIG_GEAR_LEVER_FULL_DOWN) return false;

    counter &= SIG_GEAR_LEVER_COUNTER_MASK;
    memset(frame, 0, sizeof(CanFrame));
    frame->id = CAN_ID_SCCM_RSTALK;
    frame->dlc = 3;
    frame->data[1] = (uint8_t)((gear_pos << SIG_GEAR_LEVER_POS_SHIFT) | counter);
    frame->data[2] = 0x00u;
    frame->data[0] = NEUTRAL_CRC_BY_COUNTER[counter] ^ POSITION_CRC_XOR[gear_pos];
    return true;
}
