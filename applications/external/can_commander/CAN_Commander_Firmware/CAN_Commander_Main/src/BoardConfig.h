#pragma once

#include <Arduino.h>

// Some libraries still gate ESP32 support on ESP32 instead of ARDUINO_ARCH_ESP32.
#if defined(ARDUINO_ARCH_ESP32) && !defined(ESP32)
#define ESP32
#endif

// =============================================================================
// Hardware configuration
//
// Edit the defaults below, or override any BOARDCFG_* value at build time, e.g.:
//   arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc
//     --build-property "compiler.cpp.extra_flags=-DBOARDCFG_CAN1_DRIVER=0 -DBOARDCFG_CAN0_TX_PIN=5" .
// Use compiler.cpp.extra_flags; overriding build.extra_flags drops the board's own
// USB/CDC flags.
// =============================================================================

// CAN1 backends: 0 = none, 1 = MCP2517FD/MCP2518FD (ACAN2517FD), 2 = MCP2515 (autowp-mcp2515)
#define BOARDCFG_CAN1_DRIVER_NONE 0
#define BOARDCFG_CAN1_DRIVER_MCP2517FD 1
#define BOARDCFG_CAN1_DRIVER_MCP2515 2

// Backward-compatible names for the Flipper UART pin overrides.
#if defined(FLIPPER_UART_TX_PIN) && !defined(BOARDCFG_FLIPPER_UART_TX_PIN)
#define BOARDCFG_FLIPPER_UART_TX_PIN FLIPPER_UART_TX_PIN
#endif
#if defined(FLIPPER_UART_RX_PIN) && !defined(BOARDCFG_FLIPPER_UART_RX_PIN)
#define BOARDCFG_FLIPPER_UART_RX_PIN FLIPPER_UART_RX_PIN
#endif

// ---- Per-target defaults --------------------------------------------------
#if defined(CONFIG_IDF_TARGET_ESP32S3)
// Official CAN Commander board (ESP32-S3).
#ifndef BOARDCFG_CAN1_DRIVER
#define BOARDCFG_CAN1_DRIVER BOARDCFG_CAN1_DRIVER_MCP2517FD
#endif
#ifndef BOARDCFG_NEOPIXEL_ENABLED
#define BOARDCFG_NEOPIXEL_ENABLED 1
#endif
#ifndef BOARDCFG_CAN0_TX_PIN
#define BOARDCFG_CAN0_TX_PIN 2
#endif
#ifndef BOARDCFG_CAN0_RX_PIN
#define BOARDCFG_CAN0_RX_PIN 1
#endif
#ifndef BOARDCFG_SPI_CS_PIN
#define BOARDCFG_SPI_CS_PIN 10
#endif
#ifndef BOARDCFG_SPI_MOSI_PIN
#define BOARDCFG_SPI_MOSI_PIN 11
#endif
#ifndef BOARDCFG_SPI_SCK_PIN
#define BOARDCFG_SPI_SCK_PIN 12
#endif
#ifndef BOARDCFG_SPI_MISO_PIN
#define BOARDCFG_SPI_MISO_PIN 13
#endif
#ifndef BOARDCFG_NEOPIXEL_PIN
#define BOARDCFG_NEOPIXEL_PIN 8
#endif
#ifndef BOARDCFG_NEOPIXEL_COUNT
#define BOARDCFG_NEOPIXEL_COUNT 2
#endif
#ifndef BOARDCFG_FLIPPER_UART_TX_PIN
#define BOARDCFG_FLIPPER_UART_TX_PIN 43
#endif
#ifndef BOARDCFG_FLIPPER_UART_RX_PIN
#define BOARDCFG_FLIPPER_UART_RX_PIN 44
#endif

#elif defined(CONFIG_IDF_TARGET_ESP32)
// Generic ESP32 DevKit + 3.3 V CAN transceiver (e.g. SN65HVD230). No second
// controller and no NeoPixels by default. GPIO6-11 are reserved for flash.
#ifndef BOARDCFG_CAN1_DRIVER
#define BOARDCFG_CAN1_DRIVER BOARDCFG_CAN1_DRIVER_NONE
#endif
#ifndef BOARDCFG_NEOPIXEL_ENABLED
#define BOARDCFG_NEOPIXEL_ENABLED 0
#endif
#ifndef BOARDCFG_CAN0_TX_PIN
#define BOARDCFG_CAN0_TX_PIN 5
#endif
#ifndef BOARDCFG_CAN0_RX_PIN
#define BOARDCFG_CAN0_RX_PIN 4
#endif
#ifndef BOARDCFG_SPI_CS_PIN
#define BOARDCFG_SPI_CS_PIN 15
#endif
#ifndef BOARDCFG_SPI_MOSI_PIN
#define BOARDCFG_SPI_MOSI_PIN 23
#endif
#ifndef BOARDCFG_SPI_SCK_PIN
#define BOARDCFG_SPI_SCK_PIN 18
#endif
#ifndef BOARDCFG_SPI_MISO_PIN
#define BOARDCFG_SPI_MISO_PIN 19
#endif
#ifndef BOARDCFG_NEOPIXEL_PIN
#define BOARDCFG_NEOPIXEL_PIN 2
#endif
#ifndef BOARDCFG_NEOPIXEL_COUNT
#define BOARDCFG_NEOPIXEL_COUNT 1
#endif
#ifndef BOARDCFG_FLIPPER_UART_TX_PIN
#define BOARDCFG_FLIPPER_UART_TX_PIN 17
#endif
#ifndef BOARDCFG_FLIPPER_UART_RX_PIN
#define BOARDCFG_FLIPPER_UART_RX_PIN 16
#endif
#endif

#if !defined(BOARDCFG_CAN1_DRIVER) || !defined(BOARDCFG_NEOPIXEL_ENABLED) || \
    !defined(BOARDCFG_CAN0_TX_PIN) || !defined(BOARDCFG_CAN0_RX_PIN) ||       \
    !defined(BOARDCFG_SPI_CS_PIN) || !defined(BOARDCFG_SPI_MOSI_PIN) ||       \
    !defined(BOARDCFG_SPI_SCK_PIN) || !defined(BOARDCFG_SPI_MISO_PIN) ||      \
    !defined(BOARDCFG_NEOPIXEL_PIN) || !defined(BOARDCFG_NEOPIXEL_COUNT) ||   \
    !defined(BOARDCFG_FLIPPER_UART_TX_PIN) || !defined(BOARDCFG_FLIPPER_UART_RX_PIN)
#error "No default pin map for this ESP32 target. Define BOARDCFG_CAN1_DRIVER, BOARDCFG_NEOPIXEL_ENABLED, BOARDCFG_CAN0_TX_PIN, BOARDCFG_CAN0_RX_PIN, BOARDCFG_SPI_{CS,MOSI,SCK,MISO}_PIN, BOARDCFG_NEOPIXEL_PIN, BOARDCFG_NEOPIXEL_COUNT and BOARDCFG_FLIPPER_UART_{TX,RX}_PIN."
#endif

// ---- Target-independent defaults ------------------------------------------

// Crystal on the CAN1 controller, in MHz.
// MCP2517FD/MCP2518FD: 4 (uses the 10x PLL), 20 or 40. MCP2515: 8, 16 or 20.
#ifndef BOARDCFG_CAN1_CRYSTAL_MHZ
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
#define BOARDCFG_CAN1_CRYSTAL_MHZ 8
#else
#define BOARDCFG_CAN1_CRYSTAL_MHZ 20
#endif
#endif

// CAN1 interrupt pin; 255 = polled (no interrupt line).
#ifndef BOARDCFG_CAN1_INT_PIN
#define BOARDCFG_CAN1_INT_PIN 255
#endif

// 1 = Wi-Fi AP + TCP CLI compiled in, 0 = compiled out
#ifndef BOARDCFG_WIFI_AP_ENABLED
#define BOARDCFG_WIFI_AP_ENABLED 1
#endif

#ifndef BOARDCFG_WIFI_AP_SSID
#define BOARDCFG_WIFI_AP_SSID "CAN_Commander"
#endif

#ifndef BOARDCFG_WIFI_AP_PASSWORD
#define BOARDCFG_WIFI_AP_PASSWORD "cancommander"
#endif

#ifndef BOARDCFG_WIFI_AP_CHANNEL
#define BOARDCFG_WIFI_AP_CHANNEL 6
#endif

#ifndef BOARDCFG_WIFI_AP_MAX_CLIENTS
#define BOARDCFG_WIFI_AP_MAX_CLIENTS 1
#endif

#ifndef BOARDCFG_WIFI_CLI_PORT
#define BOARDCFG_WIFI_CLI_PORT 7777
#endif

// 1 = AP auto-start enabled by default at first boot (can be overridden and persisted at runtime)
// Default: 0 (user must run `wifi start` or persist `wifi set autostart=1`)
#ifndef BOARDCFG_WIFI_AP_AUTOSTART
#define BOARDCFG_WIFI_AP_AUTOSTART 0
#endif

#define BOARDCFG_FIRMWARE_VERSION_STRING "2.3"

namespace BoardConfig {

static constexpr uint8_t FIRMWARE_VERSION_MAJOR = 2;
static constexpr uint8_t FIRMWARE_VERSION_MINOR = 3;
static constexpr const char *FIRMWARE_VERSION_STRING = BOARDCFG_FIRMWARE_VERSION_STRING;

static constexpr uint8_t CAN1_DRIVER_NONE = BOARDCFG_CAN1_DRIVER_NONE;
static constexpr uint8_t CAN1_DRIVER_MCP2517FD = BOARDCFG_CAN1_DRIVER_MCP2517FD;
static constexpr uint8_t CAN1_DRIVER_MCP2515 = BOARDCFG_CAN1_DRIVER_MCP2515;
static constexpr uint8_t CAN1_DRIVER = BOARDCFG_CAN1_DRIVER;
static constexpr uint8_t CAN1_CRYSTAL_MHZ = BOARDCFG_CAN1_CRYSTAL_MHZ;
static constexpr uint8_t NEOPIXEL_ENABLED = BOARDCFG_NEOPIXEL_ENABLED;
static constexpr uint8_t WIFI_AP_ENABLED = BOARDCFG_WIFI_AP_ENABLED;
static constexpr const char *WIFI_AP_SSID = BOARDCFG_WIFI_AP_SSID;
static constexpr const char *WIFI_AP_PASSWORD = BOARDCFG_WIFI_AP_PASSWORD;
static constexpr uint8_t WIFI_AP_CHANNEL = BOARDCFG_WIFI_AP_CHANNEL;
static constexpr uint8_t WIFI_AP_MAX_CLIENTS = BOARDCFG_WIFI_AP_MAX_CLIENTS;
static constexpr uint16_t WIFI_CLI_PORT = BOARDCFG_WIFI_CLI_PORT;
static constexpr uint8_t WIFI_AP_AUTOSTART = BOARDCFG_WIFI_AP_AUTOSTART;
static constexpr bool CAN1_ENABLED = (CAN1_DRIVER != CAN1_DRIVER_NONE);

static constexpr gpio_num_t CAN0_TX_PIN = (gpio_num_t)BOARDCFG_CAN0_TX_PIN;
static constexpr gpio_num_t CAN0_RX_PIN = (gpio_num_t)BOARDCFG_CAN0_RX_PIN;

static constexpr int SPI_CS_PIN = BOARDCFG_SPI_CS_PIN;
static constexpr int SPI_MOSI_PIN = BOARDCFG_SPI_MOSI_PIN;
static constexpr int SPI_SCK_PIN = BOARDCFG_SPI_SCK_PIN;
static constexpr int SPI_MISO_PIN = BOARDCFG_SPI_MISO_PIN;
static constexpr uint8_t CAN1_INT_PIN = BOARDCFG_CAN1_INT_PIN;
static constexpr uint32_t CAN1_SPI_CLOCK_HZ = 10000000UL;

static constexpr int NEOPIXEL_PIN = BOARDCFG_NEOPIXEL_PIN;
static constexpr uint16_t NEOPIXEL_COUNT = BOARDCFG_NEOPIXEL_COUNT;

static constexpr int FLIPPER_UART_TX = BOARDCFG_FLIPPER_UART_TX_PIN;
static constexpr int FLIPPER_UART_RX = BOARDCFG_FLIPPER_UART_RX_PIN;

static constexpr uint32_t CAN0_BITRATE = 500000;
static constexpr uint32_t CAN1_BITRATE = 500000;

static constexpr uint32_t FLIPPER_UART_BAUD = 921600;
static constexpr uint32_t USB_CLI_BAUD = 115200;

static constexpr uint16_t MAX_DBC_SIGNALS = 128;
static constexpr uint16_t EVENT_QUEUE_DEPTH_PER_SINK = 256;
static constexpr uint32_t FLIPPER_OWNER_TIMEOUT_MS = 10000;

static constexpr uint16_t MAX_FLIPPER_PAYLOAD = 220;
// ASCII argument buffers must hold a full Flipper payload plus a terminator.
static constexpr uint16_t ASCII_ARGS_MAX = MAX_FLIPPER_PAYLOAD + 1;
static constexpr uint16_t CLI_LINE_MAX = 192;
static constexpr uint16_t CLI_OUT_MAX = 192;

// ---- Compile-time checks ----------------------------------------------------

static_assert(CAN1_DRIVER == CAN1_DRIVER_NONE ||
                  CAN1_DRIVER == CAN1_DRIVER_MCP2517FD ||
                  CAN1_DRIVER == CAN1_DRIVER_MCP2515,
              "Invalid BOARDCFG_CAN1_DRIVER");
static_assert(CAN1_DRIVER != CAN1_DRIVER_MCP2517FD ||
                  CAN1_CRYSTAL_MHZ == 4 || CAN1_CRYSTAL_MHZ == 20 || CAN1_CRYSTAL_MHZ == 40,
              "Invalid BOARDCFG_CAN1_CRYSTAL_MHZ for MCP2517FD/MCP2518FD (valid: 4, 20, 40)");
static_assert(CAN1_DRIVER != CAN1_DRIVER_MCP2515 ||
                  CAN1_CRYSTAL_MHZ == 8 || CAN1_CRYSTAL_MHZ == 16 || CAN1_CRYSTAL_MHZ == 20,
              "Invalid BOARDCFG_CAN1_CRYSTAL_MHZ for MCP2515 (valid: 8, 16, 20)");
static_assert(NEOPIXEL_ENABLED == 0 || NEOPIXEL_ENABLED == 1,
              "Invalid BOARDCFG_NEOPIXEL_ENABLED (valid: 0 or 1)");
static_assert(WIFI_AP_ENABLED == 0 || WIFI_AP_ENABLED == 1,
              "Invalid BOARDCFG_WIFI_AP_ENABLED (valid: 0 or 1)");
static_assert(WIFI_AP_AUTOSTART == 0 || WIFI_AP_AUTOSTART == 1,
              "Invalid BOARDCFG_WIFI_AP_AUTOSTART (valid: 0 or 1)");
static_assert((sizeof(BOARDCFG_WIFI_AP_SSID) - 1) > 0 && (sizeof(BOARDCFG_WIFI_AP_SSID) - 1) <= 32,
              "Invalid BOARDCFG_WIFI_AP_SSID length (valid: 1..32 chars)");
static_assert(
    WIFI_AP_ENABLED == 0 ||
        ((sizeof(BOARDCFG_WIFI_AP_PASSWORD) - 1) >= 8 && (sizeof(BOARDCFG_WIFI_AP_PASSWORD) - 1) <= 63),
    "Invalid BOARDCFG_WIFI_AP_PASSWORD length (valid: 8..63 chars when Wi-Fi AP is enabled)");
static_assert(WIFI_AP_CHANNEL >= 1 && WIFI_AP_CHANNEL <= 13,
              "Invalid BOARDCFG_WIFI_AP_CHANNEL (valid: 1..13)");
static_assert(WIFI_AP_MAX_CLIENTS >= 1 && WIFI_AP_MAX_CLIENTS <= 4,
              "Invalid BOARDCFG_WIFI_AP_MAX_CLIENTS (valid: 1..4)");
static_assert(WIFI_CLI_PORT > 0, "Invalid BOARDCFG_WIFI_CLI_PORT");

static_assert(BOARDCFG_CAN0_TX_PIN != BOARDCFG_CAN0_RX_PIN, "CAN0 TX and RX pins must differ");
static_assert(FLIPPER_UART_TX != FLIPPER_UART_RX, "Flipper UART TX and RX pins must differ");
static_assert(FLIPPER_UART_TX != BOARDCFG_CAN0_TX_PIN && FLIPPER_UART_TX != BOARDCFG_CAN0_RX_PIN &&
                  FLIPPER_UART_RX != BOARDCFG_CAN0_TX_PIN && FLIPPER_UART_RX != BOARDCFG_CAN0_RX_PIN,
              "Flipper UART pins overlap the CAN0 pins");
static_assert(!CAN1_ENABLED ||
                  (SPI_CS_PIN != BOARDCFG_CAN0_TX_PIN && SPI_CS_PIN != BOARDCFG_CAN0_RX_PIN &&
                   SPI_MOSI_PIN != BOARDCFG_CAN0_TX_PIN && SPI_MOSI_PIN != BOARDCFG_CAN0_RX_PIN &&
                   SPI_SCK_PIN != BOARDCFG_CAN0_TX_PIN && SPI_SCK_PIN != BOARDCFG_CAN0_RX_PIN &&
                   SPI_MISO_PIN != BOARDCFG_CAN0_TX_PIN && SPI_MISO_PIN != BOARDCFG_CAN0_RX_PIN &&
                   SPI_CS_PIN != FLIPPER_UART_TX && SPI_CS_PIN != FLIPPER_UART_RX &&
                   SPI_MOSI_PIN != FLIPPER_UART_TX && SPI_MOSI_PIN != FLIPPER_UART_RX &&
                   SPI_SCK_PIN != FLIPPER_UART_TX && SPI_SCK_PIN != FLIPPER_UART_RX &&
                   SPI_MISO_PIN != FLIPPER_UART_TX && SPI_MISO_PIN != FLIPPER_UART_RX),
              "CAN1 SPI pins overlap the CAN0 or Flipper UART pins");
static_assert(!NEOPIXEL_ENABLED ||
                  (NEOPIXEL_PIN != BOARDCFG_CAN0_TX_PIN && NEOPIXEL_PIN != BOARDCFG_CAN0_RX_PIN &&
                   NEOPIXEL_PIN != FLIPPER_UART_TX && NEOPIXEL_PIN != FLIPPER_UART_RX),
              "NeoPixel pin overlaps the CAN0 or Flipper UART pins");

#if defined(CONFIG_IDF_TARGET_ESP32)
static_assert((BOARDCFG_CAN0_TX_PIN < 6 || BOARDCFG_CAN0_TX_PIN > 11) &&
                  (BOARDCFG_CAN0_RX_PIN < 6 || BOARDCFG_CAN0_RX_PIN > 11) &&
                  (FLIPPER_UART_TX < 6 || FLIPPER_UART_TX > 11) &&
                  (FLIPPER_UART_RX < 6 || FLIPPER_UART_RX > 11) &&
                  (!NEOPIXEL_ENABLED || NEOPIXEL_PIN < 6 || NEOPIXEL_PIN > 11),
              "GPIO6-11 are connected to the flash chip on ESP32 and cannot be used");
#endif

// With "USB CDC On Boot" disabled, Serial (the USB CLI) is UART0 on the board's
// default TX/RX pins. On ESP32-S3 those are GPIO43/44, the Flipper UART pins, so
// the two would fight over the same pins. Build with CDCOnBoot=cdc
// (Arduino IDE: Tools > USB CDC On Boot > Enabled) or move the Flipper UART.
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
static_assert(FLIPPER_UART_TX != TX && FLIPPER_UART_TX != RX &&
                  FLIPPER_UART_RX != TX && FLIPPER_UART_RX != RX,
              "Flipper UART pins collide with UART0 (the USB CLI when 'USB CDC On Boot' is disabled). "
              "Build with --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc or move BOARDCFG_FLIPPER_UART_*_PIN.");
#endif

} // namespace BoardConfig
