#pragma once

// ── CAN IDs ───────────────────────────────────────────────────────────────────
#define CAN_ID_STW_ACTN_RQ    0x045u  // 69   - STW_ACTN_RQ:  steering stalk (Legacy follow distance)
#define CAN_ID_TRIP_PLANNING  0x082u  // 130  - UI_tripPlanning: precondition trigger
#define CAN_ID_BMS_HV_BUS     0x132u  // 306  - BMS_hvBusStatus: pack voltage / current
#define CAN_ID_BMS_SOC        0x292u  // 658  - BMS_socStatus:   state of charge
#define CAN_ID_BMS_THERMAL    0x312u  // 786  - BMS_thermalStatus: battery temp
#define CAN_ID_GTW_CAR_STATE  0x318u  // 792  - GTW_carState:    OTA detection
#define CAN_ID_EPAS_STATUS    0x370u  // 880  - EPAS3P_sysStatus: nag killer target
#define CAN_ID_GTW_CAR_CONFIG 0x398u  // 920  - GTW_carConfig:   HW version detection
#define CAN_ID_ISA_SPEED      0x399u  // 921  - ISA speed limit:  HW4 chime suppress
#define CAN_ID_AP_LEGACY      0x3EEu  // 1006 - DAS_autopilot:   Legacy / HW1 / HW2
#define CAN_ID_FOLLOW_DIST    0x3F8u  // 1016 - DAS_followDistance: speed profile source
#define CAN_ID_DAS_AP_CONFIG  0x331u  // 817  - DAS autopilot config (tier restore target, ~1 Hz)
#define CAN_ID_AP_CONTROL     0x3FDu  // 1021 - DAS_autopilotControl: HW3 / HW4 core
#define CAN_ID_DAS_STATUS     0x39Bu  // 923  - DAS_status: AP hands-on state (nag gating)

// ── GPIO ──────────────────────────────────────────────────────────────────────
#if defined(BOARD_LILYGO)
  #define PIN_CAN_TX         27
  #define PIN_CAN_RX         26
  #define PIN_CAN_SPEED_MODE 23   // SN65HVD230 Rs — must be LOW for TX+RX
  #define PIN_LED            4    // SK6812 NeoPixel
  #define PIN_BUTTON         0    // Boot button, active-LOW, internal pull-up
  #define SD_MISO            2
  #define SD_MOSI            15
  #define SD_SCLK            14
  #define SD_CS              13
#else
  #ifndef PIN_CAN_TX
  #define PIN_CAN_TX   22   // TWAI TX → ATOMIC CAN Base TX
  #endif
  #ifndef PIN_CAN_RX
  #define PIN_CAN_RX   19   // TWAI RX ← ATOMIC CAN Base RX
  #endif
  #ifndef PIN_LED
  #define PIN_LED      27   // SK6812 NeoPixel (single LED)
  #endif
#endif
#ifndef PIN_BUTTON
#define PIN_BUTTON    0   // Built-in button (GPIO0), active-LOW, internal pull-up
#endif

// MCP2515 SPI — only used in CAN_DRIVER_MCP2515 build (generic ESP32)
// Standard VSPI pins: SCK=18, MISO=19, MOSI=23, CS=5
#define PIN_MCP_CS   5
#define PIN_MCP_SCK  18
#define PIN_MCP_MISO 19
#define PIN_MCP_MOSI 23

// MCP2515 oscillator: common Chinese modules use 8 MHz
#define MCP_CRYSTAL_MHZ  MCP_8MHZ   // from autowp-mcp2515 CAN_CLOCK enum

// ── Timing ────────────────────────────────────────────────────────────────────
#define WIRING_WARN_MS        5000u   // Red LED / serial warning if no CAN after this
#define PRECOND_INTERVAL_MS    500u   // Re-inject 0x082 precondition every N ms
#define BMS_PRINT_MS          1000u   // BMS serial print interval
#define BUTTON_DEBOUNCE_MS        50u
#define LONG_PRESS_MS           3000u   // Long press → toggle NAG killer
#define DOUBLE_CLICK_MS          400u   // Max gap between two clicks for double-click
#define FACTORY_RESET_HOLD_MS   5000u   // Hold duration to arm factory reset
#define FACTORY_RESET_WINDOW_MS 20000u  // Clean-boot window during which reset is possible
#define STATUS_PRINT_MS       5000u   // Periodic status line when Active

// OTA detection hardening on GTW_carState (0x318)
// Some firmware versions keep non-zero states when no update is actively running.
// We only treat one specific raw value as "update in progress" and require
// consecutive-frame confirmation to avoid false positives.
#define OTA_IN_PROGRESS_RAW_VALUE  1u
#define OTA_ASSERT_FRAMES          3u
#define OTA_CLEAR_FRAMES           6u

#if defined(BOARD_LILYGO)
  #define ME2107_EN 16
#endif

// ── Deep sleep (BOARD_LILYGO only) ───────────────────────────────────────────
// SN65HVD230 RXD (= PIN_CAN_RX, GPIO 26) is an RTC-capable GPIO on ESP32.
// When the CAN bus goes dominant the pin goes LOW — used as ext0 wakeup source.
// The SN65HVD230 Rs pin has an internal 100 kΩ pull-down, so it stays in
// normal-receive mode when GPIO 23 floats during deep sleep.
#define SLEEP_IDLE_MS   120000u   // CAN silence before entering deep sleep
#define SLEEP_WARN_MS    5000u   // serial/log warning this many ms before sleep
