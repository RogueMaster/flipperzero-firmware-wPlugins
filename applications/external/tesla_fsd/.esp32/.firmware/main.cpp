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
        http_can_stream_set_enabled(!active);
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
    if (frame.id == CAN_ID_GTW_CAR_STATE)  g_state.seen_gtw_car_state++;
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) g_state.seen_gtw_car_config++;
    if (frame.id == CAN_ID_AP_CONTROL)     g_state.seen_ap_control++;
    if (frame.id == CAN_ID_BMS_HV_BUS)     g_state.seen_bms_hv++;
    if (frame.id == CAN_ID_BMS_SOC)        g_state.seen_bms_soc++;
    if (frame.id == CAN_ID_BMS_THERMAL)    g_state.seen_bms_thermal++;
    state_exit();

    can_dump_record(bus, frame);
    if (state_snapshot().op_mode == OpMode_ListenOnly) {
        http_can_stream_record(bus, frame);
    }
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
    // (same byte5[5:2]); read it as a fallback so the nag gate isn't starved.
    // Read-only and non-returning — the ISA chime-suppress path below still
    // handles 0x399 for HW4.
    if (hw_uses_hw4_das_status(das_state.hw_version) &&
        frame.id == CAN_ID_DAS_STATUS_HW3 && !das_state.das_hw4_status_seen) {
        state_enter();
        fsd_handle_das_handsonly_399(&g_state, &frame);
        state_exit();
    }

    // ── Beyond here only run when TX is allowed ───────────────────────────────
    state_enter();
    bool tx = fsd_can_transmit(&g_state);
    state_exit();

    // NAG killer — build echo only when TX is currently allowed.
    if (frame.id == CAN_ID_EPAS_STATUS) {
        CanFrame echo;
        state_enter();
        bool fired = tx ? fsd_handle_nag_killer(&g_state, &frame, &echo) : false;
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
        if (modified && tx) send_on_bus(bus, f);
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
        if (modified && tx) send_on_bus(bus, f);
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
    Serial.printf("[CFG] pins: LED=%d BUTTON=%d CAN0_TX=%d CAN0_RX=%d MCP_CS=%d MCP_SCK=%d\n",
                  PIN_LED, PIN_BUTTON, PIN_CAN_TX, PIN_CAN_RX, PIN_MCP_CS, PIN_MCP_SCK);
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
        http_can_stream_set_enabled(state_snapshot().op_mode == OpMode_ListenOnly);
        Serial.println("[SER] Type 'ip' in the serial monitor to print the WiFi address again");
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

    // ── Precondition frame injection ──────────────────────────────────────────
    static uint32_t last_precond_ms = 0;
    FSDState s = state_snapshot();
    if (s.precondition && fsd_can_transmit(&s) &&
        (now - last_precond_ms) >= PRECOND_INTERVAL_MS) {
        CanFrame pf;
        fsd_build_precondition_frame(&pf);
        send_on_bus(bus_id_from_index(PRECONDITION_TX_BUS_INDEX), pf);
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
            "[STA] HW:%-6s AP:%-4s FSD_UI:%-4s NAG:%-10s OTA:%-3s "
            "Profile:%d  RX:%lu TX:%lu Mod:%lu Err:%lu\n",
            hw_str,
            s.ap_active       ? "ON"         : "wait",
            s.fsd_enabled     ? "ON"         : "wait",
            s.nag_suppressed  ? "suppressed"  : "active",
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
    // captured at true full rate. Restore accept-all when the stream ends.
    // Only ever single in Listen-Only (the stream is disabled in Active mode).
    {
        static bool     s_hw_filter_single = false;
        static uint32_t s_hw_filter_id     = 0xFFFFFFFFu;
        uint32_t want_id = 0;
        bool want_single = http_can_stream_single_filter(&want_id);
        if (want_single != s_hw_filter_single ||
            (want_single && want_id != s_hw_filter_id)) {
            for (uint8_t i = 0; i < CAN_ACTIVE_BUS_COUNT; i++) {
                if (g_can_ok[i] && g_can[i])
                    g_can[i]->setAcceptanceFilter(want_single, want_id);
            }
            s_hw_filter_single = want_single;
            s_hw_filter_id     = want_id;
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
