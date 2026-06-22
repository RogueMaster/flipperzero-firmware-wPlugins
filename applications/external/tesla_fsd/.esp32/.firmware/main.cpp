/*
 * main.cpp — Tesla FSD Unlock for ESP32
 *
 * Port of hypery11/flipper-tesla-fsd to M5Stack ATOM Lite + ATOMIC CAN Base.
 *
 * Default state: Listen-Only (blue LED).  Press button once to go Active (green).
 *
 * Button:
 *   Single click  → toggle Listen-Only / Active
 *   Long press 3s → toggle NAG Killer on/off
 *   Double click  → toggle BMS serial output
 *
 * Serial 115200 baud.  Status prints every 5 s when Active.
 * BMS output (when enabled): voltage, current, power, SoC, temp every 1 s.
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <esp_sleep.h>
#include <esp_ota_ops.h>
#include "config.h"
#include "can_signals.h"
#include "fsd_handler.h"
#include "can_driver.h"
#include "led.h"
#include "wifi_manager.h"
#include "web_dashboard.h"
#include "can_dump.h"
#include "http_can_stream.h"
#include "prefs.h"
#if defined(BOARD_TTGO_DISPLAY)
#include "display.h"
#endif

// ── Globals ───────────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_T2CAN_DUAL)
#define CAN_ACTIVE_BUS_COUNT 2u
#else
#define CAN_ACTIVE_BUS_COUNT 1u
#endif

static CanDriver *g_can[CAN_ACTIVE_BUS_COUNT] = {};
static bool       g_can_ok[CAN_ACTIVE_BUS_COUNT] = {};       // true once begin() succeeds
static uint32_t   g_can_last_retry_ms[CAN_ACTIVE_BUS_COUNT] = {}; // periodic re-init
#define CAN_REINIT_INTERVAL_MS  30000u
static FSDState   g_state = {};
static portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;

static CanBusId bus_id_from_index(uint8_t index) {
    return index == 1 ? CAN_BUS_SECONDARY : CAN_BUS_PRIMARY;
}

static CanBusId configured_bus_from_index(uint8_t index) {
    if (index >= CAN_ACTIVE_BUS_COUNT) index = 0u;
    return bus_id_from_index(index);
}

static uint8_t bus_index(CanBusId bus) {
    return bus == CAN_BUS_SECONDARY ? 1u : 0u;
}

static void state_enter() {
    portENTER_CRITICAL(&g_state_mux);
}

static void state_exit() {
    portEXIT_CRITICAL(&g_state_mux);
}

static FSDState state_snapshot() {
    FSDState s;
    state_enter();
    s = g_state;
    state_exit();
    return s;
}

static bool hw_uses_hw3_das_status(TeslaHWVersion hw) {
    return hw == TeslaHW_Legacy || hw == TeslaHW_HW3;
}

static bool hw_uses_hw4_das_status(TeslaHWVersion hw) {
    return hw == TeslaHW_HW4;
}

static bool frame_looks_like_hw3_das_status(const CanFrame &frame) {
    if (frame.id != CAN_ID_DAS_STATUS_HW3 || frame.dlc != CAN_FRAME_MAX_DATA_LEN) return false;

    uint8_t ap_state = frame.data[SIG_DAS_HW3_AP_STATE_BYTE] & SIG_DAS_HW3_AP_STATE_MASK;
    uint8_t hands_on =
        (frame.data[SIG_DAS_HANDS_ON_STATE_BYTE] >> SIG_DAS_HANDS_ON_STATE_SHIFT) &
        SIG_DAS_HANDS_ON_STATE_MASK;

    return ap_state <= SIG_DAS_HW3_AP_ACTIVE_STATE &&
           hands_on <= SIG_DAS_HANDS_ON_SUSPENDED;
}

static bool serial_cmd_equals(const char *cmd, const char *expected) {
    while (*cmd && *expected) {
        char a = *cmd++;
        char b = *expected++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return false;
    }
    return *cmd == '\0' && *expected == '\0';
}

static void serial_command_tick() {
    static char buf[24];
    static uint8_t len = 0;

    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (len == 0) continue;
            buf[len] = '\0';
            len = 0;

            if (serial_cmd_equals(buf, "ip") || serial_cmd_equals(buf, "wifi")) {
                wifi_print_status();
            } else if (serial_cmd_equals(buf, "help") || serial_cmd_equals(buf, "?")) {
                Serial.println("[SER] Commands: ip");
            } else {
                Serial.println("[SER] Unknown command. Type: ip");
            }
            continue;
        }

        if (c < 32 || c > 126) continue;
        if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        } else {
            len = 0;
            Serial.println("[SER] Command too long");
        }
    }
}

static void can_set_all_listen_only(bool listen_only) {
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (g_can[i]) g_can[i]->setListenOnly(listen_only);
    }
}

static bool can_any_ok() {
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (g_can_ok[i]) return true;
    }
    return false;
}

static uint32_t can_total_error_count() {
    uint32_t total = 0;
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (g_can[i]) total += g_can[i]->errorCount();
    }
    return total;
}

static uint32_t can_total_tx_count() {
    uint32_t total = 0;
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (g_can[i]) total += g_can[i]->txCount();
    }
    return total;
}

static CanDriver *can_for_bus(CanBusId bus) {
    uint8_t index = bus_index(bus);
    if (index >= CAN_ACTIVE_BUS_COUNT) return nullptr;
    return g_can[index];
}

static bool send_on_bus(CanBusId bus, const CanFrame &frame) {
    CanDriver *driver = can_for_bus(bus);
    return driver ? driver->send(frame) : false;
}

static bool send_generated_frame(CanBusId bus, const CanFrame &frame) {
    return send_on_bus(bus, frame);
}

static bool send_modified_frame(CanBusId bus, const CanFrame &frame) {
    return send_on_bus(bus, frame);
}

static uint8_t g_last_gear_counter = 0;
static bool g_last_gear_counter_valid = false;
static uint32_t g_last_gear_counter_ms = 0;

enum ContinuousApFlowState : uint8_t {
    ContAp_Idle = 0,
    ContAp_WaitSignalOff,
    ContAp_WaitApReady,
    ContAp_Attempting,
};

static ContinuousApFlowState g_cont_ap_state = ContAp_Idle;
static uint32_t g_cont_ap_signal_off_ms = 0;
static uint32_t g_cont_ap_attempt_ms = 0;
static uint32_t g_cont_ap_torque_high_ms = 0;
static uint32_t g_cont_ap_last_brake_ms = 0;
static uint32_t g_cont_ap_last_stalk_full_up_ms = 0;
static uint8_t g_cont_ap_attempts = 0;
static bool g_cont_ap_last_ap_active = false;

static constexpr uint8_t GEAR_SEQUENCE_MAX = 4;
static uint8_t g_gear_sequence[GEAR_SEQUENCE_MAX] = {};
static uint8_t g_gear_sequence_len = 0;
static uint8_t g_gear_sequence_index = 0;
static uint32_t g_gear_sequence_progress_ms = 0;
static uint32_t g_gear_sequence_next_ms = 0;
static bool g_gear_sequence_counter_valid = false;
static uint8_t g_gear_sequence_counter = 0;
static uint32_t g_gear_sequence_send_fail_count = 0;

enum GearSequenceSendResult : uint8_t {
    GearSeqSend_Sent = 0,
    GearSeqSend_Inactive,
    GearSeqSend_WaitStep,
    GearSeqSend_WaitCounter,
    GearSeqSend_TxBlocked,
    GearSeqSend_BuildFailed,
    GearSeqSend_TxFailed,
};

static CanBusId preferred_generated_bus(uint32_t frame_id) {
    if (frame_id == CAN_ID_SCCM_RSTALK)
        return configured_bus_from_index(GEAR_LEVER_TX_BUS_INDEX);
    return CAN_BUS_PRIMARY;
}

static bool gear_sequence_active() {
    return g_gear_sequence_index < g_gear_sequence_len;
}

static void clear_gear_sequence() {
    g_gear_sequence_len = 0;
    g_gear_sequence_index = 0;
    g_gear_sequence_progress_ms = 0;
    g_gear_sequence_next_ms = 0;
    g_gear_sequence_counter_valid = false;
    g_gear_sequence_counter = 0;
    g_gear_sequence_send_fail_count = 0;
}

static bool cached_gear_counter_is_fresh(uint32_t now) {
    return g_last_gear_counter_valid &&
           g_last_gear_counter_ms != 0u &&
           (uint32_t)(now - g_last_gear_counter_ms) <= GEAR_LEVER_CACHED_COUNTER_MAX_AGE_MS;
}

static const char *gear_sequence_send_result_name(GearSequenceSendResult result) {
    switch (result) {
        case GearSeqSend_Sent:        return "sent";
        case GearSeqSend_Inactive:    return "inactive";
        case GearSeqSend_WaitStep:    return "wait_step";
        case GearSeqSend_WaitCounter: return "wait_counter";
        case GearSeqSend_TxBlocked:   return "tx_blocked";
        case GearSeqSend_BuildFailed: return "build_failed";
        case GearSeqSend_TxFailed:    return "tx_failed";
        default:                      return "?";
    }
}

static GearSequenceSendResult send_gear_position_for_sequence(uint8_t gear_pos,
                                                              const char *name) {
    (void)name;
    uint32_t now = millis();
    FSDState s = state_snapshot();
    if (!fsd_can_transmit(&s)) return GearSeqSend_TxBlocked;

    if (!g_gear_sequence_counter_valid) {
        if (!cached_gear_counter_is_fresh(now)) return GearSeqSend_WaitCounter;
        g_gear_sequence_counter = g_last_gear_counter;
        g_gear_sequence_counter_valid = true;
    }

    CanFrame f;
    uint8_t next_counter =
        (uint8_t)((g_gear_sequence_counter + 1u) & SIG_GEAR_LEVER_COUNTER_MASK);
    if (!fsd_build_gear_lever_frame(&f, gear_pos, next_counter)) {
        return GearSeqSend_BuildFailed;
    }

    bool sent = send_generated_frame(preferred_generated_bus(CAN_ID_SCCM_RSTALK), f);
    if (sent) {
        g_gear_sequence_counter = next_counter;
        g_last_gear_counter = next_counter;
        g_last_gear_counter_ms = now;
        g_last_gear_counter_valid = true;
        return GearSeqSend_Sent;
    }
    return GearSeqSend_TxFailed;
}

static GearSequenceSendResult send_next_gear_sequence_frame(const char *name) {
    if (!gear_sequence_active()) return GearSeqSend_Inactive;
    uint8_t gear_pos = g_gear_sequence[g_gear_sequence_index];
    GearSequenceSendResult result = send_gear_position_for_sequence(gear_pos, name);
    if (result != GearSeqSend_Sent) {
        g_gear_sequence_send_fail_count++;
        return result;
    }
    g_gear_sequence_index++;
    uint32_t now = millis();
    g_gear_sequence_progress_ms = now;
    if (!gear_sequence_active()) clear_gear_sequence();
    else g_gear_sequence_next_ms = now + GEAR_SEQUENCE_STEP_MS;
    return GearSeqSend_Sent;
}

static bool gear_sequence_timed_out(uint32_t now) {
    return gear_sequence_active() &&
           g_gear_sequence_progress_ms != 0u &&
           (uint32_t)(now - g_gear_sequence_progress_ms) > GEAR_SEQUENCE_TIMEOUT_MS;
}

static GearSequenceSendResult gear_sequence_tick(uint32_t now, const char *name) {
    if (!gear_sequence_active()) return GearSeqSend_Inactive;
    if (g_gear_sequence_next_ms != 0u &&
        (int32_t)(now - g_gear_sequence_next_ms) < 0) {
        return GearSeqSend_WaitStep;
    }
    return send_next_gear_sequence_frame(name);
}

static GearSequenceSendResult arm_gear_ap_double_press_sequence(uint32_t now) {
    g_gear_sequence[0] = SIG_GEAR_LEVER_FULL_DOWN;
    g_gear_sequence[1] = SIG_GEAR_LEVER_CENTER;
    g_gear_sequence[2] = SIG_GEAR_LEVER_FULL_DOWN;
    g_gear_sequence[3] = SIG_GEAR_LEVER_CENTER;
    g_gear_sequence_len = 4;
    g_gear_sequence_index = 0;
    g_gear_sequence_progress_ms = now;
    g_gear_sequence_next_ms = now;
    return gear_sequence_tick(now, "CONT-AP");
}

#if defined(CAN_DRIVER_T2CAN_DUAL)
static void debug_log_bus_stats() {
    Serial.printf("[CAN] RX can0=%lu can1=%lu TX can0=%lu can1=%lu Err can0=%lu can1=%lu\n",
                  (unsigned long)(g_can[0] ? g_can[0]->rxCount() : 0),
                  (unsigned long)(g_can[1] ? g_can[1]->rxCount() : 0),
                  (unsigned long)(g_can[0] ? g_can[0]->txCount() : 0),
                  (unsigned long)(g_can[1] ? g_can[1]->txCount() : 0),
                  (unsigned long)(g_can[0] ? g_can[0]->errorCount() : 0),
                  (unsigned long)(g_can[1] ? g_can[1]->errorCount() : 0));
}
#endif

static const char *hw_to_str(TeslaHWVersion hw) {
    switch (hw) {
        case TeslaHW_HW4:    return "HW4";
        case TeslaHW_HW3:    return "HW3";
        case TeslaHW_Legacy: return "Legacy";
        default:             return "?";
    }
}

static uint8_t epas_hands_on_level(const CanFrame &frame) {
    return (frame.data[SIG_EPAS_HANDS_ON_BYTE] >> SIG_EPAS_HANDS_ON_SHIFT) &
           SIG_EPAS_HANDS_ON_MASK;
}

static uint8_t epas_counter(const CanFrame &frame) {
    return frame.data[SIG_EPAS_COUNTER_BYTE] & SIG_EPAS_COUNTER_MASK;
}

static void debug_log_das_status(CanBusId bus, uint32_t source_id, const FSDState &state) {
    static bool initialized = false;
    static uint32_t last_ms = 0;
    static CanBusId last_bus = CAN_BUS_PRIMARY;
    static uint32_t last_source_id = 0;
    static TeslaHWVersion last_hw = TeslaHW_Unknown;
    static bool last_ap_active = false;
    static uint8_t last_ap_state = 0;
    static uint8_t last_hands_state = 0;

    uint32_t now = millis();
    bool changed =
        !initialized ||
        last_bus != bus ||
        last_source_id != source_id ||
        last_hw != state.hw_version ||
        last_ap_active != state.ap_active ||
        last_ap_state != state.das_ap_state ||
        last_hands_state != state.das_hands_on_state;

    if (!changed && (now - last_ms) < 5000u) return;

    Serial.printf("[DAS] bus=%s src=0x%03lX hw=%s ap=%s ap_state=%u hands=%u lane=%u cnt=%u chk=0x%02X\n",
                  can_bus_name(bus),
                  (unsigned long)source_id,
                  hw_to_str(state.hw_version),
                  state.ap_active ? "ON" : "wait",
                  state.das_ap_state,
                  state.das_hands_on_state,
                  state.das_lane_change_state,
                  state.das_counter,
                  state.das_checksum);

    initialized = true;
    last_ms = now;
    last_bus = bus;
    last_source_id = source_id;
    last_hw = state.hw_version;
    last_ap_active = state.ap_active;
    last_ap_state = state.das_ap_state;
    last_hands_state = state.das_hands_on_state;
}

static void debug_log_bms_seen(CanBusId bus, uint32_t frame_id, const FSDState &state) {
    static bool seen[CAN_ACTIVE_BUS_COUNT][3] = {};
    uint8_t bus_i = bus_index(bus);
    uint8_t id_i;
    if (frame_id == CAN_ID_BMS_HV_BUS) {
        id_i = 0;
    } else if (frame_id == CAN_ID_BMS_SOC) {
        id_i = 1;
    } else if (frame_id == CAN_ID_BMS_THERMAL) {
        id_i = 2;
    } else {
        return;
    }
    if (bus_i >= CAN_ACTIVE_BUS_COUNT || seen[bus_i][id_i]) return;
    seen[bus_i][id_i] = true;

    Serial.printf("[BMS] first %s frame=0x%03lX hv=%lu soc=%lu thermal=%lu\n",
                  can_bus_name(bus),
                  (unsigned long)frame_id,
                  (unsigned long)state.seen_bms_hv,
                  (unsigned long)state.seen_bms_soc,
                  (unsigned long)state.seen_bms_thermal);
}

typedef enum {
    NagDebug_Disabled = 0,
    NagDebug_ApInactive,
    NagDebug_HandsOk,
    NagDebug_DasSatisfied,
    NagDebug_NotFired,
    NagDebug_TxBlocked,
    NagDebug_BuiltNoTx,
    NagDebug_TxFailed,
    NagDebug_TxEcho,
} NagDebugReason;

static const char *nag_debug_reason_name(NagDebugReason reason) {
    switch (reason) {
        case NagDebug_Disabled:     return "disabled";
        case NagDebug_ApInactive:   return "ap_inactive";
        case NagDebug_HandsOk:      return "hands_ok";
        case NagDebug_DasSatisfied: return "das_satisfied";
        case NagDebug_NotFired:     return "not_fired";
        case NagDebug_TxBlocked:    return "tx_blocked";
        case NagDebug_BuiltNoTx:    return "built_no_tx";
        case NagDebug_TxFailed:     return "tx_failed";
        case NagDebug_TxEcho:       return "tx_echo";
        default:                    return "?";
    }
}

static NagDebugReason nag_debug_reason(const FSDState &state,
                                       uint8_t epas_hands,
                                       bool fired,
                                       bool tx_allowed,
                                       bool sent) {
    if (fired) {
        if (sent) return NagDebug_TxEcho;
        return tx_allowed ? NagDebug_TxFailed : NagDebug_BuiltNoTx;
    }
    if (!tx_allowed) return NagDebug_TxBlocked;
    if (!state.nag_killer) return NagDebug_Disabled;
    if (!state.ap_active) return NagDebug_ApInactive;
    if (epas_hands == SIG_EPAS_HANDS_ON_OK) return NagDebug_HandsOk;
    if (state.das_seen &&
        (state.das_hands_on_state == SIG_DAS_HANDS_ON_NOT_REQUIRED ||
         state.das_hands_on_state == SIG_DAS_HANDS_ON_SUSPENDED)) {
        return NagDebug_DasSatisfied;
    }
    return NagDebug_NotFired;
}

static void debug_log_nag_decision(CanBusId bus,
                                   const CanFrame &frame,
                                   const CanFrame &echo,
                                   bool fired,
                                   bool tx_allowed,
                                   bool sent,
                                   const FSDState &before,
                                   const FSDState &after) {
    static bool initialized = false;
    static uint32_t last_ms = 0;
    static CanBusId last_bus = CAN_BUS_PRIMARY;
    static NagDebugReason last_reason = NagDebug_NotFired;
    static uint8_t last_epas_hands = 0xFFu;
    static uint8_t last_das_hands = 0xFFu;
    static bool last_ap_active = false;

    uint32_t now = millis();
    uint8_t hands = epas_hands_on_level(frame);
    NagDebugReason reason = nag_debug_reason(before, hands, fired, tx_allowed, sent);
    bool nag_relevant = fired || hands != SIG_EPAS_HANDS_ON_OK;
    bool changed =
        !initialized ||
        last_bus != bus ||
        last_reason != reason ||
        last_epas_hands != hands ||
        last_das_hands != before.das_hands_on_state ||
        last_ap_active != before.ap_active;

    if (!nag_relevant && !changed) return;
    if (!changed && (now - last_ms) < 1000u) return;

    if (fired) {
        Serial.printf("[NAG] bus=%s %s epas_lvl=%u cnt=%u tx=%u ap=%u das_seen=%u das=%u echo_cnt=%u echo_chk=0x%02X echoes=%lu\n",
                      can_bus_name(bus),
                      nag_debug_reason_name(reason),
                      hands,
                      epas_counter(frame),
                      tx_allowed ? 1 : 0,
                      before.ap_active ? 1 : 0,
                      before.das_seen ? 1 : 0,
                      before.das_hands_on_state,
                      epas_counter(echo),
                      echo.data[7],
                      (unsigned long)after.nag_echo_count);
    } else {
        Serial.printf("[NAG] bus=%s skip=%s epas_lvl=%u cnt=%u tx=%u ap=%u das_seen=%u das=%u echoes=%lu\n",
                      can_bus_name(bus),
                      nag_debug_reason_name(reason),
                      hands,
                      epas_counter(frame),
                      tx_allowed ? 1 : 0,
                      before.ap_active ? 1 : 0,
                      before.das_seen ? 1 : 0,
                      before.das_hands_on_state,
                      (unsigned long)after.nag_echo_count);
    }

    initialized = true;
    last_ms = now;
    last_bus = bus;
    last_reason = reason;
    last_epas_hands = hands;
    last_das_hands = before.das_hands_on_state;
    last_ap_active = before.ap_active;
}

static void apply_detected_hw(TeslaHWVersion hw, const char *reason) {
    if (hw == TeslaHW_Unknown) return;
    state_enter();
    if (g_state.hw_version == hw) {
        state_exit();
        return;
    }
    fsd_apply_hw_version(&g_state, hw);
    state_exit();

    const char *hw_str =
        (hw == TeslaHW_HW4) ? "HW4" :
        (hw == TeslaHW_HW3) ? "HW3" : "Legacy";
    Serial.printf("[HW] Auto-detected: %s (%s)\n", hw_str, reason);
    can_dump_log("HW  auto-detected: %s (%s)", hw_str, reason);
}

static const char *continuous_ap_state_name(ContinuousApFlowState state) {
    switch (state) {
        case ContAp_WaitSignalOff: return "wait_signal_off";
        case ContAp_WaitApReady:   return "wait_ap_ready";
        case ContAp_Attempting:    return "attempting";
        case ContAp_Idle:
        default:                   return "idle";
    }
}

static void continuous_ap_reset(const char *reason) {
    if (g_cont_ap_state != ContAp_Idle || g_cont_ap_attempts != 0u) {
        Serial.printf("[CONT-AP] stop: %s (state=%s attempts=%u)\n",
                      reason,
                      continuous_ap_state_name(g_cont_ap_state),
                      (unsigned)g_cont_ap_attempts);
    }
    g_cont_ap_state = ContAp_Idle;
    g_cont_ap_signal_off_ms = 0u;
    g_cont_ap_attempt_ms = 0u;
    g_cont_ap_torque_high_ms = 0u;
    g_cont_ap_attempts = 0u;
    clear_gear_sequence();
}

static bool continuous_ap_turn_signal_active(const FSDState &s) {
    return s.turn_status_seen && (s.left_turn_active || s.right_turn_active);
}

static bool continuous_ap_turn_signal_off(const FSDState &s) {
    return s.turn_status_seen && !s.left_turn_active && !s.right_turn_active;
}

static bool continuous_ap_steering_torque_high(const FSDState &s) {
    if (!s.torsion_bar_torque_seen) return false;
    return s.torsion_bar_torque_nm >= CONT_AP_STEERING_TORQUE_ABORT_NM ||
           s.torsion_bar_torque_nm <= -CONT_AP_STEERING_TORQUE_ABORT_NM;
}

static bool continuous_ap_torque_allows(uint32_t now, const FSDState &s) {
    if (!continuous_ap_steering_torque_high(s)) {
        g_cont_ap_torque_high_ms = 0u;
        return true;
    }

    if (g_cont_ap_torque_high_ms == 0u) {
        g_cont_ap_torque_high_ms = now;
        if (g_cont_ap_state != ContAp_Idle) {
            Serial.printf("[CONT-AP] steering torque high %.2f Nm; waiting\n",
                          s.torsion_bar_torque_nm);
        }
    }

    if (g_cont_ap_state != ContAp_Idle &&
        (uint32_t)(now - g_cont_ap_torque_high_ms) > CONT_AP_STEERING_TORQUE_TIMEOUT_MS) {
        Serial.printf("[CONT-AP] steering torque high timeout %.2f Nm\n",
                      s.torsion_bar_torque_nm);
        continuous_ap_reset("steering torque high");
    }
    return false;
}

static bool continuous_ap_brake_recent(uint32_t now, const FSDState &s) {
    if (s.driver_brake_applied) return true;
    return g_cont_ap_last_brake_ms != 0u &&
           (uint32_t)(now - g_cont_ap_last_brake_ms) <= CONT_AP_BRAKE_RECENT_MS;
}

static bool continuous_ap_brake_allows(uint32_t now, const FSDState &s) {
    // Fail closed: the brake kill switch reads 0x145 (ESP_status). Until at least
    // one 0x145 frame is seen we cannot prove the pedal is released, so refuse to
    // engage rather than silently dropping the interlock on a bus without 0x145.
    if (!s.brake_status_seen) {
        if (g_cont_ap_state != ContAp_Idle) {
            Serial.println("[CONT-AP] brake status (0x145) not seen; refusing to engage");
            continuous_ap_reset("brake status unseen");
        }
        return false;
    }
    if (!continuous_ap_brake_recent(now, s)) return true;

    if (g_cont_ap_state != ContAp_Idle) {
        Serial.println("[CONT-AP] brake pedal kill switch");
        continuous_ap_reset("brake pedal");
    }
    return false;
}

static bool continuous_ap_stalk_stop_recent(uint32_t now, const FSDState &s) {
    uint32_t last = s.stalk_full_up_ms;
    if (g_cont_ap_last_stalk_full_up_ms != 0u &&
        (last == 0u || (int32_t)(g_cont_ap_last_stalk_full_up_ms - last) > 0)) {
        last = g_cont_ap_last_stalk_full_up_ms;
    }
    return last != 0u && (uint32_t)(now - last) <= CONT_AP_STALK_STOP_RECENT_MS;
}

static bool continuous_ap_stalk_stop_allows(uint32_t now, const FSDState &s) {
    if (!continuous_ap_stalk_stop_recent(now, s)) return true;

    if (g_cont_ap_state != ContAp_Idle) {
        Serial.println("[CONT-AP] right stalk full-up kill switch");
        continuous_ap_reset("right stalk full-up");
    }
    return false;
}

static bool continuous_ap_hw3_legacy_try_start_attempt(uint32_t now) {
    if (!cached_gear_counter_is_fresh(now)) {
        return false;
    }

    GearSequenceSendResult result = arm_gear_ap_double_press_sequence(now);
    if (result != GearSeqSend_Sent) {
        clear_gear_sequence();
        return false;
    }

    g_cont_ap_attempts++;
    g_cont_ap_attempt_ms = now;
    g_cont_ap_state = ContAp_Attempting;
    Serial.printf("[CONT-AP] attempt %u/%u: 0x229 full-down double press started on %s\n",
                  (unsigned)g_cont_ap_attempts,
                  (unsigned)CONT_AP_MAX_RETRIES,
                  can_bus_name(preferred_generated_bus(CAN_ID_SCCM_RSTALK)));
    return true;
}

static void continuous_ap_tick_hw3_legacy(uint32_t now,
                                          const FSDState &s,
                                          bool ap_disabled_now) {
    bool torque_allows = continuous_ap_torque_allows(now, s);
    bool brake_allows = continuous_ap_brake_allows(now, s);
    bool stalk_stop_allows = continuous_ap_stalk_stop_allows(now, s);

    if (ap_disabled_now) {
        Serial.printf("[CONT-AP] AP disabled torque=%s%.2f Nm brake=%u stalk_stop=%u turn_active=%u left=%u right=%u\n",
                      s.torsion_bar_torque_seen ? "" : "unseen:",
                      s.torsion_bar_torque_seen ? s.torsion_bar_torque_nm : 0.0f,
                      continuous_ap_brake_recent(now, s) ? 1u : 0u,
                      continuous_ap_stalk_stop_recent(now, s) ? 1u : 0u,
                      continuous_ap_turn_signal_active(s) ? 1u : 0u,
                      s.left_turn_active ? 1u : 0u,
                      s.right_turn_active ? 1u : 0u);
    }

    if (ap_disabled_now && continuous_ap_turn_signal_active(s)) {
        if (!brake_allows) {
            Serial.println("[CONT-AP] AP disabled with turn signal active, but brake pedal was pressed");
            return;
        }
        if (!stalk_stop_allows) {
            Serial.println("[CONT-AP] AP disabled with turn signal active, but right stalk full-up was pressed");
            return;
        }
        if (!torque_allows) {
            Serial.printf("[CONT-AP] AP disabled with turn signal active, but steering torque is high %.2f Nm\n",
                          s.torsion_bar_torque_nm);
            return;
        }
        g_cont_ap_state = ContAp_WaitSignalOff;
        g_cont_ap_signal_off_ms = 0u;
        g_cont_ap_attempt_ms = 0u;
        g_cont_ap_attempts = 0u;
        clear_gear_sequence();
        Serial.println("[CONT-AP] AP disabled while turn signal active; waiting for turn signal off");
    }

    switch (g_cont_ap_state) {
        case ContAp_Idle:
            return;

        case ContAp_WaitSignalOff:
            if (s.ap_active) {
                continuous_ap_reset("AP already active");
                return;
            }
            if (!brake_allows) return;
            if (!stalk_stop_allows) return;
            if (!torque_allows) return;
            if (continuous_ap_turn_signal_off(s)) {
                g_cont_ap_signal_off_ms = now;
                g_cont_ap_state = ContAp_WaitApReady;
                Serial.printf("[CONT-AP] turn signal off; waiting %lu ms before AP reengage\n",
                              (unsigned long)CONT_AP_REENGAGE_DELAY_MS);
            }
            return;

        case ContAp_WaitApReady:
            if (s.ap_active) {
                continuous_ap_reset("AP already active");
                return;
            }
            if (g_cont_ap_attempts == 0u &&
                (uint32_t)(now - g_cont_ap_signal_off_ms) > CONT_AP_READY_WAIT_TIMEOUT_MS) {
                continuous_ap_reset("AP ready/counter timeout");
                return;
            }
            if (!brake_allows) return;
            if (!stalk_stop_allows) return;
            if (!torque_allows) return;
            if ((uint32_t)(now - g_cont_ap_signal_off_ms) < CONT_AP_REENGAGE_DELAY_MS) {
                return;
            }
            if (s.ap_ready) {
                continuous_ap_hw3_legacy_try_start_attempt(now);
            }
            return;

        case ContAp_Attempting:
            if (!brake_allows) return;
            if (!stalk_stop_allows) return;
            if (!torque_allows) return;
            {
                GearSequenceSendResult result = gear_sequence_tick(now, "CONT-AP");
                uint32_t sequence_now = millis();
                if (gear_sequence_timed_out(sequence_now)) {
                    Serial.printf("[CONT-AP] 0x229 sequence timeout after %u/%u frames (last=%s fails=%lu)\n",
                                  (unsigned)g_gear_sequence_index,
                                  (unsigned)g_gear_sequence_len,
                                  gear_sequence_send_result_name(result),
                                  (unsigned long)g_gear_sequence_send_fail_count);
                    clear_gear_sequence();
                }
            }
            if (gear_sequence_active()) {
                return;
            }
            if (s.ap_active) {
                continuous_ap_reset("AP active");
                return;
            }
            uint32_t attempt_elapsed = now - g_cont_ap_attempt_ms;
            if (attempt_elapsed < CONT_AP_ATTEMPT_RESULT_MS) {
                return;
            }
            if (g_cont_ap_attempts >= CONT_AP_MAX_RETRIES) {
                continuous_ap_reset("retry limit");
                return;
            }
            if (attempt_elapsed < (CONT_AP_ATTEMPT_RESULT_MS + CONT_AP_RETRY_DELAY_MS)) {
                return;
            }
            if (!s.ap_ready) {
                g_cont_ap_state = ContAp_WaitApReady;
                return;
            }
            continuous_ap_hw3_legacy_try_start_attempt(now);
            return;
    }
}

static void continuous_ap_hw4_start_attempt(uint32_t now) {
    (void)now;
    // TODO: implement HW4 re-engage frame sequence once HW4 control signals are identified.
}

static bool continuous_ap_hw4_reengage_allowed(uint32_t now, const FSDState &s) {
    (void)now;
    (void)s;
    // TODO: implement HW4-specific kill switches and preconditions.
    return false;
}

static void continuous_ap_tick_hw4(uint32_t now,
                                   const FSDState &s,
                                   bool ap_disabled_now) {
    // TODO: implement HW4 Continuous AP using HW4 turn/AP controls instead of stalk frames.
    if (ap_disabled_now &&
        continuous_ap_turn_signal_active(s) &&
        continuous_ap_hw4_reengage_allowed(now, s)) {
        continuous_ap_hw4_start_attempt(now);
    }
}

static void continuous_ap_tick(uint32_t now) {
    FSDState s = state_snapshot();
    bool ap_disabled_now = g_cont_ap_last_ap_active && !s.ap_active;
    g_cont_ap_last_ap_active = s.ap_active;

    if (!s.continuous_ap) {
        continuous_ap_reset("disabled");
        return;
    }

    if (!fsd_can_transmit(&s)) {
        continuous_ap_reset("TX disabled");
        return;
    }

    switch (s.hw_version) {
        case TeslaHW_HW4:
            continuous_ap_tick_hw4(now, s, ap_disabled_now);
            return;
        case TeslaHW_HW3:
        case TeslaHW_Legacy:
            continuous_ap_tick_hw3_legacy(now, s, ap_disabled_now);
            return;
        case TeslaHW_Unknown:
        default:
            continuous_ap_reset("HW unknown");
            return;
    }
}

// ── Button state machine ──────────────────────────────────────────────────────
static uint32_t g_btn_down_ms     = 0;
static uint32_t g_last_release_ms = 0;
static bool     g_btn_down        = false;
static int      g_pending_clicks  = 0;
static bool     g_long_fired      = false;  // prevent double-fire on long press
static bool     g_btn_ignore_boot = true;   // wait for release after boot
static bool     g_factory_reset_window   = false;  // set true on clean boot, clears at 20s
static bool     g_factory_reset_eligible = false;  // latched at leading edge if press was in window
static bool     g_factory_reset_armed    = false;  // blink done, waiting for release

#if defined(BOARD_TTGO_DISPLAY)
static uint32_t g_display_last_wake_ms = 0;
static bool     g_last_fsd_enabled     = false;

static uint32_t g_btn2_down_ms     = 0;
static uint32_t g_btn2_release_ms  = 0;
static bool     g_btn2_down        = false;
static bool     g_btn2_ignore_boot = true;
#endif

#if defined(BOARD_LILYGO)
static uint32_t g_last_can_rx_ms = 0;
static bool     g_sleep_warned   = false;
#endif

static void dispatch_clicks(int n) {
    if (n == 1) {
        // Toggle Listen-Only ↔ Active
        FSDState saved;
        bool active = false;
        state_enter();
        if (g_state.op_mode == OpMode_ListenOnly) {
            g_state.op_mode = OpMode_Active;
            active = true;
        } else {
            g_state.op_mode = OpMode_ListenOnly;
        }
        saved = g_state;
        state_exit();
        can_set_all_listen_only(!active);
        http_can_stream_set_enabled(true);  // capture works in both modes now (#108)
        Serial.println(active ? "[BTN] → Active mode" : "[BTN] → Listen-Only mode");
        can_dump_log(active ? "MODE switched to Active — TX enabled" : "MODE switched to Listen-Only — TX disabled");
        prefs_save(&saved);
    } else if (n >= 2) {
        // Toggle BMS serial output
        FSDState saved;
        state_enter();
        g_state.bms_output = !g_state.bms_output;
        bool enabled = g_state.bms_output;
        saved = g_state;
        state_exit();
        Serial.printf("[BTN] BMS output: %s\n", enabled ? "ON" : "OFF");
        prefs_save(&saved);
    }
}

static void button_tick() {
    bool pressed = (digitalRead(PIN_BUTTON) == LOW);
    uint32_t now = millis();

    if (g_btn_ignore_boot) {
        if (!pressed) g_btn_ignore_boot = false;
        return;
    }

    if (pressed && !g_btn_down) {
        // Leading edge — debounce
        if ((now - g_last_release_ms) < BUTTON_DEBOUNCE_MS) return;
        g_btn_down             = true;
        g_btn_down_ms          = now;
        g_long_fired           = false;
        g_factory_reset_eligible = g_factory_reset_window;  // latch at press time
#if defined(BOARD_TTGO_DISPLAY)
        display_wake();
        g_display_last_wake_ms = now;
#endif
    }

    if (g_btn_down && pressed && !g_long_fired) {
        uint32_t held = now - g_btn_down_ms;
        if (g_factory_reset_eligible && held >= FACTORY_RESET_HOLD_MS) {
            g_long_fired           = true;
            g_pending_clicks       = 0;
            g_factory_reset_armed  = true;
            Serial.println("[BTN] Factory reset armed — release to confirm");
            led_factory_blink();
        } else if (!g_factory_reset_eligible && held >= LONG_PRESS_MS) {
            g_long_fired      = true;
            g_pending_clicks  = 0;
            FSDState saved;
            state_enter();
            g_state.nag_killer = !g_state.nag_killer;
            bool enabled = g_state.nag_killer;
            saved = g_state;
            state_exit();
            Serial.printf("[BTN] NAG Killer: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
        // eligible press with held < FACTORY_RESET_HOLD_MS: suppress 3s NAG killer
    }

    if (!pressed && g_btn_down) {
        // Trailing edge
        g_btn_down               = false;
        g_last_release_ms        = now;
        g_factory_reset_eligible = false;
        if (g_factory_reset_armed) {
            Serial.println("[BTN] Factory reset confirmed — clearing NVS");
            prefs_clear();
            delay(200);
            ESP.restart();
        }
        if (!g_long_fired) {
            g_pending_clicks++;
        }
    }

    // Flush pending clicks after the double-click window closes
    if (g_pending_clicks > 0 && !g_btn_down &&
        (now - g_last_release_ms) >= DOUBLE_CLICK_MS) {
        dispatch_clicks(g_pending_clicks);
        g_pending_clicks = 0;
    }

#if defined(BOARD_TTGO_DISPLAY)
    bool pressed2 = (digitalRead(PIN_BUTTON2) == LOW);
    if (g_btn2_ignore_boot) {
        if (!pressed2) g_btn2_ignore_boot = false;
    } else {
        if (pressed2 && !g_btn2_down) {
            if ((now - g_btn2_release_ms) >= BUTTON_DEBOUNCE_MS) {
                g_btn2_down = true;
                g_btn2_down_ms = now;
            }
        }
        if (!pressed2 && g_btn2_down) {
            g_btn2_down = false;
            g_btn2_release_ms = now;
            if (display_is_awake()) {
                display_sleep();
            } else {
                display_wake();
                g_display_last_wake_ms = now;
            }
        }
    }
#endif
}

// ── LED refresh ───────────────────────────────────────────────────────────────
static void update_led() {
    FSDState s = state_snapshot();
    if (g_factory_reset_armed) {
        led_set(LED_WHITE);
        return;
    }
    if (s.rx_count == 0 && millis() > WIRING_WARN_MS) {
        led_set(LED_RED);
    } else if (s.tesla_ota_in_progress) {
        led_set(LED_YELLOW);
    } else if (s.op_mode == OpMode_Active) {
        led_set(LED_GREEN);
    } else {
        led_set(LED_BLUE);
    }
}

// ── CAN frame dispatcher ──────────────────────────────────────────────────────
static void process_frame(CanBusId bus, const CanFrame &frame) {
    state_enter();
    g_state.rx_count++;
    // AP-First stability debounce: stamp the last time AP was not engaged, so
    // fsd_ap_first_allows() can require AP held stable for AP_FIRST_STABLE_MS (#100/#108).
    if (g_state.das_ap_state < 2u) g_state.ap_unstable_tick_ms = millis();
    if (frame.id == CAN_ID_GTW_CAR_STATE)  g_state.seen_gtw_car_state++;
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) g_state.seen_gtw_car_config++;
    if (frame.id == CAN_ID_AP_CONTROL)     g_state.seen_ap_control++;
    if (frame.id == CAN_ID_BMS_HV_BUS)     g_state.seen_bms_hv++;
    if (frame.id == CAN_ID_BMS_SOC)        g_state.seen_bms_soc++;
    if (frame.id == CAN_ID_BMS_THERMAL)    g_state.seen_bms_thermal++;
    state_exit();

    can_dump_record(bus, frame);
    // Record to the web stream in BOTH modes so a capture can run *through* an
    // Activate (needed to catch the steer-jerk transient, #108). Recording is a
    // cheap ring-buffer write and only does anything when a client is connected.
    // Injection safety is preserved by never installing the single-ID hardware
    // filter in Active mode (see the acceptance-filter block in loop()).
    http_can_stream_record(bus, frame);
#if defined(BOARD_LILYGO)
    g_last_can_rx_ms = millis();
    g_sleep_warned   = false;
#endif

    // DLC sanity: skip zero-length frames
    if (frame.dlc == 0) return;

    // ── HW auto-detect (passive, runs in both modes) ─────────────────────────
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) {
        TeslaHWVersion hw = fsd_detect_hw_version(&frame);
        FSDState s = state_snapshot();
        if (hw != TeslaHW_Unknown && s.hw_version == TeslaHW_Unknown)
            apply_detected_hw(hw, "0x398");
        return;
    }

    // ── OTA monitoring (always, mode-independent) ─────────────────────────────
    if (frame.id == CAN_ID_GTW_CAR_STATE) {
        state_enter();
        bool was_ota = g_state.tesla_ota_in_progress;
        fsd_handle_gtw_car_state(&g_state, &frame);
        bool is_ota = g_state.tesla_ota_in_progress;
        bool ignore_ota = g_state.ignore_ota;
        uint8_t raw = g_state.ota_raw_state;
        state_exit();
        if (!was_ota && is_ota) {
            if (ignore_ota) {
                Serial.printf("[OTA] Update in progress (raw=%u) - TX allowed by Ignore OTA\n", raw);
                can_dump_log("OTA  started - TX allowed by Ignore OTA");
            } else {
                Serial.printf("[OTA] Update in progress (raw=%u) - TX suspended\n", raw);
                can_dump_log("OTA  started - TX suspended");
            }
        } else if (was_ota && !is_ota) {
            Serial.printf("[OTA] Update finished (raw=%u) - TX resumed\n", raw);
            can_dump_log("OTA  finished - TX resumed");
        }
        return;
    }

    // ── BMS sniff (read-only, always) ─────────────────────────────────────────
    if (frame.id == CAN_ID_BMS_HV_BUS)  { state_enter(); fsd_handle_bms_hv(&g_state, &frame);      state_exit(); return; }
    if (frame.id == CAN_ID_BMS_SOC)     { state_enter(); fsd_handle_bms_soc(&g_state, &frame);     state_exit(); return; }
    if (frame.id == CAN_ID_BMS_THERMAL) { state_enter(); fsd_handle_bms_thermal(&g_state, &frame); state_exit(); return; }

    // ── DAS status (read-only, always) — gating for NAG killer ───────────────
    FSDState das_state = state_snapshot();
    if (hw_uses_hw3_das_status(das_state.hw_version) && frame.id == CAN_ID_DAS_STATUS_HW3) {
        state_enter();
        fsd_handle_das_status_hw3(&g_state, &frame);
        state_exit();
        return;
    }
    if (hw_uses_hw4_das_status(das_state.hw_version) && frame.id == CAN_ID_DAS_STATUS_HW4) {
        state_enter();
        fsd_handle_das_status_hw4(&g_state, &frame);
        state_exit();
        return;
    }
    // HW4 trims that never broadcast 0x39B carry the hands-on field on 0x399
    // (same byte5[5:2]); read it as a fallback so the nag gate isn't starved (#100).
    // Read-only and non-returning — the ISA chime-suppress path still handles 0x399.
    if (hw_uses_hw4_das_status(das_state.hw_version) &&
        frame.id == CAN_ID_DAS_STATUS_HW3 && !das_state.das_hw4_status_seen) {
        state_enter();
        fsd_handle_das_handsonly_399(&g_state, &frame);
        state_exit();
    }

    // ── Continuous AP HW3/Legacy state parsers (read-only, always) ──────────
    if (frame.id == CAN_ID_SCCM_RSTALK) {
        FSDState s = state_snapshot();
        if (s.hw_version != TeslaHW_HW3 && s.hw_version != TeslaHW_Legacy) return;
        uint32_t now_ms = millis();
        if (frame.dlc > SIG_GEAR_LEVER_POS_BYTE) {
            uint8_t gear_pos =
                (frame.data[SIG_GEAR_LEVER_POS_BYTE] >> SIG_GEAR_LEVER_POS_SHIFT) &
                SIG_GEAR_LEVER_POS_MASK;
            if (gear_pos == SIG_GEAR_LEVER_FULL_UP) {
                g_cont_ap_last_stalk_full_up_ms = now_ms;
            }
        }
        state_enter();
        fsd_handle_gear_lever(&g_state, &frame, now_ms);
        if (g_state.stalk_full_up_ms != 0u &&
            (g_cont_ap_last_stalk_full_up_ms == 0u ||
             (int32_t)(g_state.stalk_full_up_ms - g_cont_ap_last_stalk_full_up_ms) > 0)) {
            g_cont_ap_last_stalk_full_up_ms = g_state.stalk_full_up_ms;
        }
        state_exit();
        if (frame.dlc > SIG_GEAR_LEVER_COUNTER_BYTE) {
            g_last_gear_counter =
                frame.data[SIG_GEAR_LEVER_COUNTER_BYTE] & SIG_GEAR_LEVER_COUNTER_MASK;
            g_last_gear_counter_valid = true;
            g_last_gear_counter_ms = now_ms;
        }
        if (frame.dlc > SIG_GEAR_LEVER_COUNTER_BYTE) {
            if (gear_sequence_active() && fsd_can_transmit(&s)) gear_sequence_tick(now_ms, "CONT-AP");
        }
        return;
    }
    if (frame.id == CAN_ID_UI_MAP_DATA) {
        state_enter();
        fsd_handle_ui_map_data(&g_state, &frame, millis());
        state_exit();
        return;
    }
    if (frame.id == CAN_ID_DAS_STATUS2) {
        state_enter();
        fsd_handle_das_status2(&g_state, &frame, millis());
        state_exit();
        return;
    }
    if (frame.id == CAN_ID_DAS_CONTROL) {
        state_enter();
        fsd_handle_das_control(&g_state, &frame);
        state_exit();
        return;
    }
    if (frame.id == CAN_ID_VCFRONT_LIGHT) {
        state_enter();
        fsd_handle_vcfront_lighting(&g_state, &frame);
        state_exit();
        return;
    }
    if (frame.id == CAN_ID_ESP_STATUS) {
        uint32_t now_ms = millis();
        state_enter();
        fsd_handle_esp_status(&g_state, &frame);
        if (g_state.driver_brake_applied) g_cont_ap_last_brake_ms = now_ms;
        state_exit();
        return;
    }

    // ── Beyond here only run when TX is allowed ───────────────────────────────
    state_enter();
    bool tx = fsd_can_transmit(&g_state);
    // AP-First (#100/#108): when enabled, hold AP/FSD/nag injection until AP is
    // engaged and stable. Gates 0x3FD / 0x3EE / 0x370 below; off by default.
    bool ap_ok = fsd_ap_first_allows(&g_state, millis());
    state_exit();

    // NAG killer — build echo only when TX is currently allowed.
    if (frame.id == CAN_ID_EPAS_STATUS) {
        CanFrame echo;
        state_enter();
        fsd_handle_epas_status(&g_state, &frame);
        bool fired = (tx && ap_ok) ? fsd_handle_nag_killer(&g_state, &frame, &echo, millis()) : false;
        state_exit();
        if (fired) {
            uint8_t lvl     = (frame.data[4] >> 6) & 0x03;
            uint8_t cnt_in  = frame.data[6] & 0x0F;
            uint8_t cnt_out = echo.data[6] & 0x0F;
            // fired is already gated on tx above, so the send is unconditional
            // here; route through the bus-aware helper from the dual-CAN work.
            can_dump_log("NAG 0x370 hands_off lvl=%u cnt=%u->%u TX echo",
                         lvl, cnt_in, cnt_out);
            send_on_bus(bus, echo);
        }
        return;
    }

    // Legacy stalk (0x045) — updates speed_profile, no TX
    if (frame.id == CAN_ID_STW_ACTN_RQ && state_snapshot().hw_version == TeslaHW_Legacy) {
        state_enter();
        fsd_handle_legacy_stalk(&g_state, &frame);
        state_exit();
        return;
    }

    // Legacy autopilot control (0x3EE)
    if (frame.id == CAN_ID_AP_LEGACY && state_snapshot().hw_version == TeslaHW_Legacy) {
        CanFrame f = frame;
        state_enter();
        bool modified = fsd_handle_legacy_autopilot(&g_state, &f);
        state_exit();
        if (modified && tx && ap_ok) send_on_bus(bus, f);
        return;
    }

    // Auto-upgrade Legacy→HW3: Palladium S/X with HW3 reports das_hw=0
    // (→Legacy) but actually uses 0x3FD. True Legacy never broadcasts 0x3FD.
    if (state_snapshot().hw_version == TeslaHW_Legacy && frame.id == CAN_ID_AP_CONTROL) {
        apply_detected_hw(TeslaHW_HW3, "upgrade:Legacy→HW3(0x3FD seen)");
    }

    // Fallback HW detection when 0x398 is unavailable on the tapped bus.
    // Prefer explicit HW4 DAS_status (0x39B) when present. If the tap only sees
    // HW3-style DAS_status on 0x399, classify as HW3 after repeated plausible
    // samples so 0x399 can be parsed for AP/NAG gating.
    static uint32_t hw_fallback_3fd_count = 0;
    static uint32_t hw_fallback_399_count = 0;
    if (state_snapshot().hw_version == TeslaHW_Unknown) {
        if (frame.id == CAN_ID_AP_LEGACY) {
            apply_detected_hw(TeslaHW_Legacy, "fallback:0x3EE");
        } else if (frame.id == CAN_ID_DAS_STATUS_HW4 && frame.dlc == CAN_FRAME_MAX_DATA_LEN) {
            apply_detected_hw(TeslaHW_HW4, "fallback:0x39B");
            hw_fallback_3fd_count = 0;
            hw_fallback_399_count = 0;
        } else if (frame_looks_like_hw3_das_status(frame)) {
            hw_fallback_399_count++;
            if (hw_fallback_399_count >= 2u)
                apply_detected_hw(TeslaHW_HW3, "fallback:0x399(DAS status)");
        } else if (frame.id == CAN_ID_AP_CONTROL) {
            hw_fallback_3fd_count++;
            if (hw_fallback_3fd_count >= 10u)
                apply_detected_hw(TeslaHW_HW3, "fallback:0x3FD(confirmed)");
        }
    }

    // ISA speed chime (0x399, HW4 only)
    FSDState s = state_snapshot();
    if (frame.id == CAN_ID_ISA_SPEED &&
        s.hw_version == TeslaHW_HW4 &&
        s.suppress_speed_chime) {
        CanFrame f = frame;
        if (fsd_handle_isa_speed_chime(&f) && tx)
            send_on_bus(bus, f);
        return;
    }

    // Follow distance → speed_profile (0x3F8), no TX
    if (frame.id == CAN_ID_FOLLOW_DIST) {
        state_enter();
        fsd_handle_follow_distance(&g_state, &frame);
        state_exit();
        return;
    }

    // TLSSC Restore (0x331) — DAS config spoof
    if (frame.id == CAN_ID_DAS_AP_CONFIG) {
        CanFrame f = frame;
        state_enter();
        bool modified = fsd_handle_tlssc_restore(&g_state, &f);
        state_exit();
        if (modified && tx) send_on_bus(bus, f);
        return;
    }

    // HW3/HW4 autopilot control (0x3FD) — main FSD activation frame
    if (frame.id == CAN_ID_AP_CONTROL) {
        CanFrame f = frame;
        state_enter();
        bool modified = fsd_handle_autopilot_frame(&g_state, &f);
        state_exit();
        if (modified && tx && ap_ok) send_on_bus(bus, f);
        return;
    }
}

#if defined(BOARD_LILYGO)
// ── Deep-sleep watchdog (Lilygo only) ────────────────────────────────────────
static void sleep_tick(uint32_t now) {
    if (now < g_last_can_rx_ms) return;
    uint32_t idle_ms = now - g_last_can_rx_ms;
    FSDState s = state_snapshot();

    if (idle_ms >= s.sleep_idle_ms) {
        Serial.printf("[SLEEP] Entering deep sleep after %lu ms CAN silence\n",
                      (unsigned long)idle_ms);
        can_dump_stop();
        sd_syslog_close();
        led_set(LED_SLEEP);
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_CAN_RX, 0);
        esp_deep_sleep_start();
        // never returns
    } else if (!g_sleep_warned && idle_ms >= (s.sleep_idle_ms - SLEEP_WARN_MS)) {
        g_sleep_warned = true;
        uint32_t remaining_ms = s.sleep_idle_ms - idle_ms;
        Serial.printf("[SLEEP] Warning: %lu ms idle, sleeping in %lu ms\n",
                      (unsigned long)idle_ms, (unsigned long)remaining_ms);
    }
}
#endif

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
#if defined(BOARD_LILYGO)
    g_last_can_rx_ms = millis();
#endif
    Serial.begin(115200);
    delay(300);

    Serial.println("\n============================");
    Serial.println(" Tesla FSD Unlock — ESP32   ");
    Serial.println("============================");
    Serial.printf("[FSD] Build: %s %s\n", __DATE__, __TIME__);

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        Serial.printf("[OTA] Running from: %s\n", running->label);

        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
                Serial.println("[OTA] First boot after update - marking as valid...");
                if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                    Serial.println("[OTA] Firmware marked valid");
                } else {
                    Serial.println("[OTA] WARNING: Could not mark firmware valid");
                }
            } else if (ota_state == ESP_OTA_IMG_VALID) {
                Serial.println("[OTA] Running verified firmware");
            }
        }
    }

#if defined(CAN_DRIVER_T2CAN_DUAL)
    Serial.println("[CAN] Driver: LilyGO T-2CAN dual CAN (TWAI can0 + MCP2515 can1)");
#elif defined(CAN_DRIVER_TWAI)
  #if defined(BOARD_WAVESHARE_S3)
    Serial.println("[CAN] Driver: ESP32-S3 TWAI (Waveshare ESP32-S3-RS485-CAN)");
  #elif defined(BOARD_LILYGO)
    Serial.println("[CAN] Driver: ESP32 TWAI (LilyGO T-CAN485)");
  #else
    Serial.println("[CAN] Driver: ESP32 TWAI (M5Stack ATOM Lite + ATOMIC CAN Base)");
  #endif
#elif defined(CAN_DRIVER_MCP2515)
    Serial.println("[CAN] Driver: MCP2515 via SPI");
#endif

#if defined(BOARD_LILYGO)
    pinMode(ME2107_EN, OUTPUT);
    digitalWrite(ME2107_EN, HIGH);
    delay(100); // Wait for 5V rail to stabilize (SD power)
    // CAN transceiver slope/mode pin — must be LOW for normal TX+RX operation.
    // Floating or HIGH puts the SN65HVD230/TJA1051 into standby (RX-only),
    // which causes the TWAI controller to go bus-off the first time it tries to TX.
    pinMode(PIN_CAN_SPEED_MODE, OUTPUT);
    digitalWrite(PIN_CAN_SPEED_MODE, LOW);
#endif

#if defined(CAN_DRIVER_T2CAN_DUAL)
    Serial.printf("[CFG] pins: LED=%d BUTTON=%d can0_TX=%d can0_RX=%d can1_CS=%d can1_SCK=%d can1_MISO=%d can1_MOSI=%d\n",
                  PIN_LED, PIN_BUTTON, PIN_CAN_TX, PIN_CAN_RX,
                  PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO, PIN_MCP_MOSI);
    Serial.printf("[CFG] TX route: modified frames -> source bus, generated precondition -> %s\n",
                  can_bus_name(configured_bus_from_index(PRECONDITION_TX_BUS_INDEX)));
    Serial.printf("[CFG] Continuous AP gear TX route: 0x229 -> %s\n",
                  can_bus_name(preferred_generated_bus(CAN_ID_SCCM_RSTALK)));
#elif defined(CAN_DRIVER_TWAI)
    Serial.printf("[CFG] pins: LED=%d BUTTON=%d CAN_TX=%d CAN_RX=%d\n",
                  PIN_LED, PIN_BUTTON, PIN_CAN_TX, PIN_CAN_RX);
#else
    Serial.printf("[CFG] pins: LED=%d BUTTON=%d MCP_CS=%d MCP_SCK=%d\n",
                  PIN_LED, PIN_BUTTON, PIN_MCP_CS, PIN_MCP_SCK);
#endif

    pinMode(PIN_BUTTON, INPUT_PULLUP);
#if defined(BOARD_TTGO_DISPLAY)
    pinMode(PIN_BUTTON2, INPUT_PULLUP);
#endif
    led_init();
#if defined(BOARD_TTGO_DISPLAY)
    display_init();
#endif

    fsd_state_init(&g_state, TeslaHW_Unknown);
    // Explicit safe defaults — will be overridden after HW auto-detect
    g_state.op_mode               = OpMode_ListenOnly;
    g_state.nag_killer            = true;
    g_state.suppress_speed_chime  = true;
    g_state.ignore_ota            = false;
    g_state.fsd_unlock            = false;
    g_state.emergency_vehicle_detect = false;
    g_state.force_fsd             = false;
    g_state.china_mode            = false;
    g_state.bms_output            = false;

    prefs_load(&g_state);
#if defined(BOARD_TTGO_DISPLAY)
    display_set_enabled(g_state.display_enabled);
#endif

    {
        esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
        g_factory_reset_window = (wakeup == ESP_SLEEP_WAKEUP_UNDEFINED);
        if (g_factory_reset_window)
            Serial.println("[BTN] Factory reset window active — hold button 5s within 20s");
    }

    if (state_snapshot().op_mode == OpMode_Active) {
        // Will be re-applied after g_can is created; record intent here only
        Serial.println("[NVS] Restored Active mode from NVS");
    }

    led_set(LED_BLUE);

    can_dump_init();

#if defined(BOARD_LILYGO)
    {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            Serial.printf("[WAKE] Woken by CAN activity (EXT0 GPIO %d)\n", PIN_CAN_RX);
        } else if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
            Serial.printf("[WAKE] Wakeup cause=%d\n", (int)cause);
        }
        g_last_can_rx_ms = millis();
    }
#endif

    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        CanBusId bus = bus_id_from_index(i);
        g_can[i] = can_driver_create(bus);
        g_can_ok[i] = g_can[i] && g_can[i]->begin(true);
        g_can_last_retry_ms[i] = millis();
    }
    if (!can_any_ok()) {
        Serial.println("[ERR] All CAN driver init failed — check wiring");
#if defined(BOARD_TTGO_DISPLAY)
        Serial.printf("[ERR] Continuing in NO-CAN mode (will retry every %lu ms)\n",
                      (unsigned long)CAN_REINIT_INTERVAL_MS);
        led_set(LED_RED);
#elif defined(CAN_DRIVER_T2CAN_DUAL)
        Serial.printf("[ERR] Continuing in NO-CAN mode (will retry every %lu ms)\n",
                      (unsigned long)CAN_REINIT_INTERVAL_MS);
        led_set(LED_RED);
#else
        // Halt: signal error via blinking red indefinitely.
        while (true) {
            led_set(LED_RED);   delay(200);
            led_set(LED_OFF);   delay(200);
        }
#endif
    } else {
        if (state_snapshot().op_mode == OpMode_Active) {
            can_set_all_listen_only(false);
            Serial.println("[CAN] 500 kbps — Active (restored from NVS)");
        } else {
            Serial.println("[CAN] 500 kbps — Listen-Only");
        }
    }
    Serial.println("[BTN] Single click : toggle Listen-Only / Active");
    Serial.println("[BTN] Long press 3s: toggle NAG Killer");
    Serial.println("[BTN] Double click : toggle BMS serial output");
    Serial.println("[LED] Blue=Listen  Green=Active  Yellow=OTA  Red=Error");

    // ── WiFi + Web dashboard (non-fatal if WiFi fails) ───────────────────────
    if (wifi_init(&g_state)) {
        web_dashboard_init(&g_state, g_can, CAN_ACTIVE_BUS_COUNT, &g_state_mux);
        http_can_stream_set_enabled(true);  // capture works in both modes now (#108)
        Serial.println("[SER] Type 'ip' in the serial monitor to print WiFi URLs again");
    }
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    if (g_factory_reset_window && now >= FACTORY_RESET_WINDOW_MS) {
        g_factory_reset_window = false;
        Serial.println("[BTN] Factory reset window closed");
    }

    button_tick();
    serial_command_tick();

    // Drain all available CAN frames in one shot
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (!g_can_ok[i] || !g_can[i]) continue;
        CanBusId bus = bus_id_from_index(i);
        CanFrame frame;
        while (g_can[i]->receive(frame)) {
            process_frame(bus, frame);
        }
        // Recover a bus-off controller so RX resumes without a manual toggle (#108).
        g_can[i]->serviceHealth();
    }

    // ── Periodic error counter refresh (~every 250 ms) ────────────────────────
    static uint32_t last_err_ms = 0;
    if ((now - last_err_ms) >= 250u) {
        state_enter();
        g_state.crc_err_count = can_total_error_count();
        g_state.tx_count      = can_total_tx_count();
        state_exit();
        last_err_ms = now;
    }

    continuous_ap_tick(now);

    // ── Precondition frame injection ──────────────────────────────────────────
    static uint32_t last_precond_ms = 0;
    FSDState s = state_snapshot();
    if (s.precondition && fsd_can_transmit(&s) &&
        (now - last_precond_ms) >= PRECOND_INTERVAL_MS) {
        CanFrame pf;
        fsd_build_precondition_frame(&pf);
        send_generated_frame(configured_bus_from_index(PRECONDITION_TX_BUS_INDEX), pf);
        last_precond_ms = now;
    }

    // ── BMS serial output ─────────────────────────────────────────────────────
    static uint32_t last_bms_ms = 0;
    s = state_snapshot();
    if (s.bms_output && s.bms_seen &&
        (now - last_bms_ms) >= BMS_PRINT_MS) {
        float kw = s.pack_voltage_v * s.pack_current_a / 1000.0f;
        Serial.printf("[BMS] %.1fV  %.1fA  %.2fkW  SoC:%.1f%%  Temp:%d~%d°C\n",
            s.pack_voltage_v,
            s.pack_current_a,
            kw,
            s.soc_percent,
            (int)s.batt_temp_min_c,
            (int)s.batt_temp_max_c);
        last_bms_ms = now;
    }

    // ── Active-mode status line ───────────────────────────────────────────────
    static uint32_t last_status_ms = 0;
    s = state_snapshot();
    if (s.op_mode == OpMode_Active &&
        (now - last_status_ms) >= STATUS_PRINT_MS) {
        const char *hw_str =
            (s.hw_version == TeslaHW_HW4)    ? "HW4"    :
            (s.hw_version == TeslaHW_HW3)    ? "HW3"    :
            (s.hw_version == TeslaHW_Legacy)  ? "Legacy" : "?";
        Serial.printf(
            "[STA] HW:%-6s AP:%-4s FSD_UI:%-4s Unlock:%-3s NAG:%-3s Echo:%lu OTA:%-3s "
            "Profile:%d  RX:%lu TX:%lu Mod:%lu Err:%lu\n",
            hw_str,
            s.ap_active       ? "ON"         : "wait",
            s.fsd_enabled     ? "ON"         : "wait",
            s.fsd_unlock      ? "ON"         : "off",
            s.nag_killer      ? "ON"         : "off",
            (unsigned long)s.nag_echo_count,
            s.tesla_ota_in_progress ? "YES"  : "no",
            s.speed_profile,
            (unsigned long)s.rx_count,
            (unsigned long)s.tx_count,
            (unsigned long)s.frames_modified,
            (unsigned long)s.crc_err_count);
        last_status_ms = now;
    }

    // ── Periodic re-init when a CAN driver failed at boot ────────────────────
    for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
        if (!g_can_ok[i] && g_can[i] &&
            (now - g_can_last_retry_ms[i]) >= CAN_REINIT_INTERVAL_MS) {
            g_can_last_retry_ms[i] = now;
            Serial.printf("[CAN] Retrying %s driver init...\n",
                          can_bus_name(bus_id_from_index(i)));
            bool listen_only = (state_snapshot().op_mode != OpMode_Active);
            g_can_ok[i] = g_can[i]->begin(listen_only);
            if (g_can_ok[i]) {
                Serial.printf("[CAN] %s re-init SUCCESS — %s mode\n",
                              can_bus_name(bus_id_from_index(i)),
                              listen_only ? "Listen-Only" : "Active");
            }
        }
    }

    // ── Wiring / hardware sanity warning ─────────────────────────────────────
    static uint32_t last_warn_ms = 0;
    s = state_snapshot();
    if (now > WIRING_WARN_MS && (now - last_warn_ms) >= 5000u) {
        if (!can_any_ok()) {
            // Driver init failed — distinguish chip-not-detected from other.
            // Skip the "no CAN traffic" warn entirely (it's never going to
            // arrive without a working driver).
            Serial.println("[WARN] CAN drivers not initialised — "
                           "no CAN traffic possible");
            last_warn_ms = now;
        } else if (s.rx_count == 0) {
            Serial.println("[WARN] No CAN traffic after 5 s — check wiring");
            Serial.println("[WARN] Verify CAN-H on OBD pin 6, CAN-L on pin 14");
            last_warn_ms = now;
        }
    }

    can_dump_tick(now);

#if defined(BOARD_LILYGO)
    sleep_tick(now);
#endif

    // ── Web dashboard (after CAN to preserve CAN frame latency) ──────────────
    web_dashboard_update();

    // ── Full-rate single-ID capture: drive the hardware acceptance filter ─────
    // When a /stream is opened with exactly one ?ids= value, restrict the CAN
    // controller to that id so its RX queue never overflows and every frame is
    // captured at true full rate. If ?bus=can0/can1 is present, scope the
    // hardware filter to that controller only. Restore accept-all when the
    // stream ends. Only ever single in Listen-Only (stream is disabled in Active).
    {
        static bool     s_hw_filter_single[CAN_ACTIVE_BUS_COUNT] = {};
        static uint32_t s_hw_filter_id[CAN_ACTIVE_BUS_COUNT] = {};
        static bool     s_hw_filter_initialized = false;
        if (!s_hw_filter_initialized) {
            for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
                s_hw_filter_id[i] = 0xFFFFFFFFu;
            }
            s_hw_filter_initialized = true;
        }

        uint32_t want_id = 0;
        bool want_single = http_can_stream_single_filter(&want_id);
        CanBusId want_bus = CAN_BUS_PRIMARY;
        bool want_bus_filter = http_can_stream_bus_filter(&want_bus);

        // Never install the single-ID hardware filter in Active mode — it would
        // restrict the controller to one id and starve injection's RX path. A
        // single-ID capture during Active silently falls back to software
        // filtering (lower rate, but injection stays intact). Multi-ID captures
        // are software-filtered anyway and run at full effect in both modes.
        bool allow_hw_filter = (state_snapshot().op_mode == OpMode_ListenOnly);
        for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
            CanBusId bus = bus_id_from_index(i);
            bool bus_want_single = want_single && allow_hw_filter && (!want_bus_filter || bus == want_bus);
            uint32_t bus_want_id = bus_want_single ? want_id : 0u;
            if (bus_want_single != s_hw_filter_single[i] ||
                (bus_want_single && bus_want_id != s_hw_filter_id[i])) {
                if (g_can_ok[i] && g_can[i]) {
                    g_can[i]->setAcceptanceFilter(bus_want_single, bus_want_id);
                    s_hw_filter_single[i] = bus_want_single;
                    s_hw_filter_id[i] = bus_want_id;
                }
            }
        }
    }

#if defined(BOARD_TTGO_DISPLAY)
    s = state_snapshot();
    display_set_enabled(s.display_enabled);
    display_set_brightness(s.display_brightness);

    if (s.fsd_enabled && !g_last_fsd_enabled) {
        display_wake();
        g_display_last_wake_ms = now;
    }
    g_last_fsd_enabled = s.fsd_enabled;

    if (display_is_awake() && s.display_timeout_s > 0) {
        if (now - g_display_last_wake_ms >= s.display_timeout_s * 1000) {
            display_sleep();
        }
    }

    display_update(&s);
#endif

    update_led();
}
