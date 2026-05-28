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
static CanDriver *g_can   = nullptr;
static bool       g_can_ok = false;       // true once g_can->begin() succeeds
static uint32_t   g_can_last_retry_ms = 0; // for periodic re-init when init fails
#define CAN_REINIT_INTERVAL_MS  30000u
static FSDState   g_state = {};
static portMUX_TYPE g_state_mux = portMUX_INITIALIZER_UNLOCKED;

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
        g_can->setListenOnly(!active);
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
static void process_frame(const CanFrame &frame) {
    state_enter();
    g_state.rx_count++;
    if (frame.id == CAN_ID_GTW_CAR_STATE)  g_state.seen_gtw_car_state++;
    if (frame.id == CAN_ID_GTW_CAR_CONFIG) g_state.seen_gtw_car_config++;
    if (frame.id == CAN_ID_AP_CONTROL)     g_state.seen_ap_control++;
    if (frame.id == CAN_ID_BMS_HV_BUS)     g_state.seen_bms_hv++;
    if (frame.id == CAN_ID_BMS_SOC)        g_state.seen_bms_soc++;
    if (frame.id == CAN_ID_BMS_THERMAL)    g_state.seen_bms_thermal++;
    state_exit();

    can_dump_record(frame);
    if (state_snapshot().op_mode == OpMode_ListenOnly) {
        http_can_stream_record(frame);
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

    // ── Beyond here only run when TX is allowed ───────────────────────────────
    state_enter();
    bool tx = fsd_can_transmit(&g_state);
    state_exit();

    // NAG killer — build echo and send before the real frame propagates (0x370)
    if (frame.id == CAN_ID_EPAS_STATUS) {
        CanFrame echo;
        state_enter();
        bool fired = fsd_handle_nag_killer(&g_state, &frame, &echo);
        state_exit();
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
        if (modified && tx) g_can->send(f);
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
            g_can->send(f);
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
        if (modified && tx) g_can->send(f);
        return;
    }

    // HW3/HW4 autopilot control (0x3FD) — main FSD activation frame
    if (frame.id == CAN_ID_AP_CONTROL) {
        CanFrame f = frame;
        state_enter();
        bool modified = fsd_handle_autopilot_frame(&g_state, &f);
        state_exit();
        if (modified && tx) g_can->send(f);
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
    delay(100); // Wait for 5V rail to stabilize (SD power)
    // CAN transceiver slope/mode pin — must be LOW for normal TX+RX operation.
    // Floating or HIGH puts the SN65HVD230/TJA1051 into standby (RX-only),
    // which causes the TWAI controller to go bus-off the first time it tries to TX.
    pinMode(PIN_CAN_SPEED_MODE, OUTPUT);
    digitalWrite(PIN_CAN_SPEED_MODE, LOW);
#endif

#if defined(CAN_DRIVER_TWAI)
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

    g_can = can_driver_create();
    g_can_ok = g_can->begin(true);
    g_can_last_retry_ms = millis();
    if (!g_can_ok) {
        Serial.println("[ERR] CAN driver init FAILED — check wiring");
#if defined(BOARD_TTGO_DISPLAY)
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
            g_can->setListenOnly(false);
            Serial.println("[CAN] 500 kbps — Active (restored from NVS)");
        } else {
            Serial.println("[CAN] 500 kbps — Listen-Only");
        }
    }
    Serial.println("[BTN] Single click : toggle Listen-Only / Active");
    Serial.println("[BTN] Long press 3s: toggle NAG Killer");
    Serial.println("[BTN] Double click : toggle BMS serial output");
    Serial.println("[LED] Blue=Listen  Green=Active  Yellow=OTA  Red=Error");

    // ── WiFi AP + Web dashboard (non-fatal if WiFi fails) ─────────────────────
    if (wifi_ap_init(&g_state)) {
        web_dashboard_init(&g_state, g_can, &g_state_mux);
        http_can_stream_set_enabled(state_snapshot().op_mode == OpMode_ListenOnly);
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

    // Drain all available CAN frames in one shot
    CanFrame frame;
    while (g_can->receive(frame)) {
        process_frame(frame);
    }

    // ── Periodic error counter refresh (~every 250 ms) ────────────────────────
    static uint32_t last_err_ms = 0;
    if ((now - last_err_ms) >= 250u) {
        state_enter();
        g_state.crc_err_count = g_can->errorCount();
        g_state.tx_count      = g_can->txCount();
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
        g_can->send(pf);
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

    // ── Periodic re-init when CAN driver failed at boot ──────────────────────
    if (!g_can_ok && g_can &&
        (now - g_can_last_retry_ms) >= CAN_REINIT_INTERVAL_MS) {
        g_can_last_retry_ms = now;
        Serial.println("[CAN] Retrying driver init…");
        bool listen_only = (state_snapshot().op_mode != OpMode_Active);
        g_can_ok = g_can->begin(listen_only);
        if (g_can_ok) {
            Serial.printf("[CAN] Re-init SUCCESS — %s mode\n",
                          listen_only ? "Listen-Only" : "Active");
        }
    }

    // ── Wiring / hardware sanity warning ─────────────────────────────────────
    static uint32_t last_warn_ms = 0;
    s = state_snapshot();
    if (now > WIRING_WARN_MS && (now - last_warn_ms) >= 5000u) {
        if (!g_can_ok) {
            // Driver init failed — distinguish chip-not-detected from other.
            // Skip the "no CAN traffic" warn entirely (it's never going to
            // arrive without a working driver).
            if (g_can && !g_can->hardwarePresent()) {
                Serial.println("[WARN] MCP2515 not detected on SPI — "
                               "no CAN traffic possible until chip responds");
            } else {
                Serial.println("[WARN] CAN driver not initialised — "
                               "no CAN traffic possible");
            }
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
