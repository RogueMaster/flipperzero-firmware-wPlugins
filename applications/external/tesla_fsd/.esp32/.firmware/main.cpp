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
#include "config.h"
#include "fsd_handler.h"
#include "can_driver.h"
#include "led.h"
#include "wifi_manager.h"
#include "web_dashboard.h"
#include "can_dump.h"

// ── Globals ───────────────────────────────────────────────────────────────────
static CanDriver *g_can   = nullptr;
static FSDState   g_state = {};

static void apply_detected_hw(TeslaHWVersion hw, const char *reason) {
    if (hw == TeslaHW_Unknown || g_state.hw_version == hw) return;

    bool old_nag    = g_state.nag_killer;
    bool old_chime  = g_state.suppress_speed_chime;
    bool old_bms    = g_state.bms_output;
    bool old_force  = g_state.force_fsd;
    OpMode old_mode = g_state.op_mode;

    fsd_state_init(&g_state, hw);
    g_state.nag_killer           = old_nag;
    g_state.suppress_speed_chime = old_chime;
    g_state.bms_output           = old_bms;
    g_state.force_fsd            = old_force;
    g_state.op_mode              = old_mode;

    const char *hw_str =
        (hw == TeslaHW_HW4) ? "HW4" :
        (hw == TeslaHW_HW3) ? "HW3" : "Legacy";
    Serial.printf("[HW] Auto-detected: %s (%s)\n", hw_str, reason);
    can_dump_log("HW  auto-detected: %s (%s)", hw_str, reason);
}

// ── Button state machine ──────────────────────────────────────────────────────
static uint32_t g_btn_down_ms     = 0;
static uint32_t g_last_release_ms = 0;
static bool     g_btn_down        = false;
static int      g_pending_clicks  = 0;
static bool     g_long_fired      = false;  // prevent double-fire on long press

static void dispatch_clicks(int n) {
    if (n == 1) {
        // Toggle Listen-Only ↔ Active
        if (g_state.op_mode == OpMode_ListenOnly) {
            g_state.op_mode = OpMode_Active;
            g_can->setListenOnly(false);
            Serial.println("[BTN] → Active mode");
            can_dump_log("MODE switched to Active — TX enabled");
        } else {
            g_state.op_mode = OpMode_ListenOnly;
            g_can->setListenOnly(true);
            Serial.println("[BTN] → Listen-Only mode");
            can_dump_log("MODE switched to Listen-Only — TX disabled");
        }
    } else if (n >= 2) {
        // Toggle BMS serial output
        g_state.bms_output = !g_state.bms_output;
        Serial.printf("[BTN] BMS output: %s\n", g_state.bms_output ? "ON" : "OFF");
    }
}

static void button_tick() {
    bool pressed = (digitalRead(PIN_BUTTON) == LOW);
    uint32_t now = millis();

    if (pressed && !g_btn_down) {
        // Leading edge — debounce
        if ((now - g_last_release_ms) < BUTTON_DEBOUNCE_MS) return;
        g_btn_down      = true;
        g_btn_down_ms   = now;
        g_long_fired    = false;
    }

    if (g_btn_down && pressed && !g_long_fired) {
        // Still held — check for long press threshold
        if ((now - g_btn_down_ms) >= LONG_PRESS_MS) {
            g_long_fired      = true;
            g_pending_clicks  = 0;  // cancel any pending click
            g_state.nag_killer = !g_state.nag_killer;
            Serial.printf("[BTN] NAG Killer: %s\n", g_state.nag_killer ? "ON" : "OFF");
        }
    }

    if (!pressed && g_btn_down) {
        // Trailing edge
        g_btn_down        = false;
        g_last_release_ms = now;
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
}

// ── LED refresh ───────────────────────────────────────────────────────────────
static void update_led() {
    if (g_state.rx_count == 0 && millis() > WIRING_WARN_MS) {
        led_set(LED_RED);
    } else if (g_state.tesla_ota_in_progress) {
        led_set(LED_YELLOW);
    } else if (g_state.op_mode == OpMode_Active) {
        led_set(LED_GREEN);
    } else {
        led_set(LED_BLUE);
    }
}

// ── CAN frame dispatcher ──────────────────────────────────────────────────────
static void process_frame(const CanFrame &frame) {
    g_state.rx_count++;
    can_dump_record(frame);

    if (frame.id == CAN_ID_GTW_CAR_STATE)  g_state.seen_gtw_car_state++;
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) g_state.seen_gtw_car_config++;
    if (frame.id == CAN_ID_AP_CONTROL)     g_state.seen_ap_control++;
    if (frame.id == CAN_ID_BMS_HV_BUS)     g_state.seen_bms_hv++;
    if (frame.id == CAN_ID_BMS_SOC)        g_state.seen_bms_soc++;
    if (frame.id == CAN_ID_BMS_THERMAL)    g_state.seen_bms_thermal++;

    // DLC sanity: skip zero-length frames
    if (frame.dlc == 0) return;

    // ── HW auto-detect (passive, runs in both modes) ─────────────────────────
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) {
        TeslaHWVersion hw = fsd_detect_hw_version(&frame);
        if (hw != TeslaHW_Unknown && g_state.hw_version == TeslaHW_Unknown)
            apply_detected_hw(hw, "0x398");
        return;
    }

    // ── OTA monitoring (always, mode-independent) ─────────────────────────────
    if (frame.id == CAN_ID_GTW_CAR_STATE) {
        bool was_ota = g_state.tesla_ota_in_progress;
        fsd_handle_gtw_car_state(&g_state, &frame);
        if (!was_ota && g_state.tesla_ota_in_progress) {
            Serial.printf("[OTA] Update in progress (raw=%u) - TX suspended\n", g_state.ota_raw_state);
            can_dump_log("OTA  started — TX suspended");
        } else if (was_ota && !g_state.tesla_ota_in_progress) {
            Serial.printf("[OTA] Update finished (raw=%u) - TX resumed\n", g_state.ota_raw_state);
            can_dump_log("OTA  finished — TX resumed");
        }
        return;
    }

    // ── BMS sniff (read-only, always) ─────────────────────────────────────────
    if (frame.id == CAN_ID_BMS_HV_BUS)  { fsd_handle_bms_hv(&g_state, &frame);      return; }
    if (frame.id == CAN_ID_BMS_SOC)     { fsd_handle_bms_soc(&g_state, &frame);     return; }
    if (frame.id == CAN_ID_BMS_THERMAL) { fsd_handle_bms_thermal(&g_state, &frame); return; }

    // ── Beyond here only run when TX is allowed ───────────────────────────────
    bool tx = fsd_can_transmit(&g_state);

    // NAG killer — build echo and send before the real frame propagates (0x370)
    if (frame.id == CAN_ID_EPAS_STATUS) {
        CanFrame echo;
        bool fired = fsd_handle_nag_killer(&g_state, &frame, &echo);
        if (fired) {
            uint8_t lvl     = (frame.data[4] >> 6) & 0x03;
            uint8_t cnt_in  = frame.data[6] & 0x0F;
            uint8_t cnt_out = echo.data[6] & 0x0F;
            can_dump_log("NAG 0x370 hands_off lvl=%u cnt=%u->%u %s",
                         lvl, cnt_in, cnt_out, tx ? "TX echo" : "listen-only no-TX");
            if (tx) g_can->send(echo);
        }
        return;
    }

    // Legacy stalk (0x045) — updates speed_profile, no TX
    if (frame.id == CAN_ID_STW_ACTN_RQ && g_state.hw_version == TeslaHW_Legacy) {
        fsd_handle_legacy_stalk(&g_state, &frame);
        return;
    }

    // Legacy autopilot control (0x3EE)
    if (frame.id == CAN_ID_AP_LEGACY && g_state.hw_version == TeslaHW_Legacy) {
        CanFrame f = frame;
        if (fsd_handle_legacy_autopilot(&g_state, &f) && tx)
            g_can->send(f);
        return;
    }

    // Fallback HW detection when 0x398 is unavailable on the tapped bus.
    if (g_state.hw_version == TeslaHW_Unknown) {
        if (frame.id == CAN_ID_AP_LEGACY) {
            apply_detected_hw(TeslaHW_Legacy, "fallback:0x3EE");
        } else if (frame.id == CAN_ID_ISA_SPEED) {
            apply_detected_hw(TeslaHW_HW4, "fallback:0x399");
        } else if (frame.id == CAN_ID_AP_CONTROL) {
            // 0x3FD exists on HW3/HW4. Prefer HW3 as safe default until 0x399 appears.
            apply_detected_hw(TeslaHW_HW3, "fallback:0x3FD");
        }
    }

    // ISA speed chime (0x399, HW4 only)
    if (frame.id == CAN_ID_ISA_SPEED &&
        g_state.hw_version == TeslaHW_HW4 &&
        g_state.suppress_speed_chime) {
        CanFrame f = frame;
        if (fsd_handle_isa_speed_chime(&f) && tx)
            g_can->send(f);
        return;
    }

    // Follow distance → speed_profile (0x3F8), no TX
    if (frame.id == CAN_ID_FOLLOW_DIST) {
        fsd_handle_follow_distance(&g_state, &frame);
        return;
    }

    // TLSSC Restore (0x331) — DAS config spoof
    if (frame.id == CAN_ID_DAS_AP_CONFIG) {
        CanFrame f = frame;
        if (fsd_handle_tlssc_restore(&g_state, &f) && tx)
            g_can->send(f);
        return;
    }

    // HW3/HW4 autopilot control (0x3FD) — main FSD activation frame
    if (frame.id == CAN_ID_AP_CONTROL) {
        CanFrame f = frame;
        if (fsd_handle_autopilot_frame(&g_state, &f) && tx)
            g_can->send(f);
        return;
    }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("\n============================");
    Serial.println(" Tesla FSD Unlock — ESP32   ");
    Serial.println("============================");
    Serial.printf("[FSD] Build: %s %s\n", __DATE__, __TIME__);
#if defined(CAN_DRIVER_TWAI)
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
    // CAN transceiver slope/mode pin — must be LOW for normal TX+RX operation.
    // Floating or HIGH puts the SN65HVD230/TJA1051 into standby (RX-only),
    // which causes the TWAI controller to go bus-off the first time it tries to TX.
    pinMode(PIN_CAN_SPEED_MODE, OUTPUT);
    digitalWrite(PIN_CAN_SPEED_MODE, LOW);
#endif

    Serial.printf("[CFG] pins: LED=%d BUTTON=%d CAN_TX=%d CAN_RX=%d\n",
                  PIN_LED, PIN_BUTTON, PIN_CAN_TX, PIN_CAN_RX);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    led_init();

    fsd_state_init(&g_state, TeslaHW_Unknown);
    // Explicit safe defaults — will be overridden after HW auto-detect
    g_state.op_mode               = OpMode_ListenOnly;
    g_state.nag_killer            = true;
    g_state.suppress_speed_chime  = true;
    g_state.emergency_vehicle_detect = false;
    g_state.force_fsd             = false;
    g_state.bms_output            = false;

    led_set(LED_BLUE);

    can_dump_init();

    g_can = can_driver_create();
    if (!g_can->begin(true)) {
        Serial.println("[ERR] CAN driver init FAILED — check wiring");
        led_set(LED_RED);
        // Halt: signal error via blinking red indefinitely
        while (true) {
            led_set(LED_RED);   delay(200);
            led_set(LED_OFF);   delay(200);
        }
    }

    Serial.println("[CAN] 500 kbps — Listen-Only");
    Serial.println("[BTN] Single click : toggle Listen-Only / Active");
    Serial.println("[BTN] Long press 3s: toggle NAG Killer");
    Serial.println("[BTN] Double click : toggle BMS serial output");
    Serial.println("[LED] Blue=Listen  Green=Active  Yellow=OTA  Red=Error");

    // ── WiFi AP + Web dashboard (non-fatal if WiFi fails) ─────────────────────
    if (wifi_ap_init()) {
        web_dashboard_init(&g_state, g_can);
    }
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();

    button_tick();

    // Drain all available CAN frames in one shot
    CanFrame frame;
    while (g_can->receive(frame)) {
        process_frame(frame);
    }

    // ── Periodic error counter refresh (~every 250 ms) ────────────────────────
    static uint32_t last_err_ms = 0;
    if ((now - last_err_ms) >= 250u) {
        g_state.crc_err_count = g_can->errorCount();
        last_err_ms = now;
    }

    // ── Precondition frame injection ──────────────────────────────────────────
    static uint32_t last_precond_ms = 0;
    if (g_state.precondition && fsd_can_transmit(&g_state) &&
        (now - last_precond_ms) >= PRECOND_INTERVAL_MS) {
        CanFrame pf;
        fsd_build_precondition_frame(&pf);
        g_can->send(pf);
        last_precond_ms = now;
    }

    // ── BMS serial output ─────────────────────────────────────────────────────
    static uint32_t last_bms_ms = 0;
    if (g_state.bms_output && g_state.bms_seen &&
        (now - last_bms_ms) >= BMS_PRINT_MS) {
        float kw = g_state.pack_voltage_v * g_state.pack_current_a / 1000.0f;
        Serial.printf("[BMS] %.1fV  %.1fA  %.2fkW  SoC:%.1f%%  Temp:%d~%d°C\n",
            g_state.pack_voltage_v,
            g_state.pack_current_a,
            kw,
            g_state.soc_percent,
            (int)g_state.batt_temp_min_c,
            (int)g_state.batt_temp_max_c);
        last_bms_ms = now;
    }

    // ── Active-mode status line ───────────────────────────────────────────────
    static uint32_t last_status_ms = 0;
    if (g_state.op_mode == OpMode_Active &&
        (now - last_status_ms) >= STATUS_PRINT_MS) {
        const char *hw_str =
            (g_state.hw_version == TeslaHW_HW4)    ? "HW4"    :
            (g_state.hw_version == TeslaHW_HW3)    ? "HW3"    :
            (g_state.hw_version == TeslaHW_Legacy)  ? "Legacy" : "?";
        Serial.printf(
            "[STA] HW:%-6s FSD:%-4s NAG:%-10s OTA:%-3s "
            "Profile:%d  RX:%lu TX:%lu Err:%lu\n",
            hw_str,
            g_state.fsd_enabled     ? "ON"         : "wait",
            g_state.nag_suppressed  ? "suppressed"  : "active",
            g_state.tesla_ota_in_progress ? "YES"  : "no",
            g_state.speed_profile,
            (unsigned long)g_state.rx_count,
            (unsigned long)g_state.frames_modified,
            (unsigned long)g_state.crc_err_count);
        last_status_ms = now;
    }

    // ── Wiring sanity warning ─────────────────────────────────────────────────
    static uint32_t last_warn_ms = 0;
    if (g_state.rx_count == 0 && now > WIRING_WARN_MS &&
        (now - last_warn_ms) >= 2000u) {
        Serial.println("[WARN] No CAN traffic after 5 s — check wiring");
        Serial.println("[WARN] Verify CAN-H on OBD pin 6, CAN-L on pin 14");
        last_warn_ms = now;
    }

    can_dump_tick(now);

    // ── Web dashboard (after CAN to preserve CAN frame latency) ──────────────
    web_dashboard_update();

    update_led();
}
