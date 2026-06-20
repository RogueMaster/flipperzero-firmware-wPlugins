#pragma once
/*
 * fsd_types.h — hardware-free CAN frame type shared by the protocol logic.
 *
 * Split out of libraries/mcp_can_2515.h so the FSD protocol core
 * (fsd_handler.c) compiles without pulling in the MCP2515 SPI driver or any
 * furi/HAL dependency. This lets the logic be unit-tested on the host (see
 * test/) and is the first step toward a single protocol core shared by the
 * Flipper and ESP32 builds.
 *
 * The CANFRAME layout is unchanged — same fields, same order, same MAX_LEN —
 * so this is behavior-preserving for the existing Flipper build.
 */

#include <stdint.h>

#define MAX_LEN 8

// CAN frame shared by the MCP2515 driver, the FSD protocol logic, and the ESP32
// firmware. The anonymous unions expose two field-name conventions over the
// same storage so both platforms keep their existing accessors while sharing
// one struct definition:
//   Flipper / mcp driver : canId / data_lenght / buffer
//   ESP32 firmware        : id    / dlc         / data   (CanFrame == CANFRAME)
// Same size and layout as before, so this is behavior-preserving for the
// existing Flipper build.
typedef struct {
    union {
        uint32_t canId;
        uint32_t id;
    };
    uint8_t ext;
    uint8_t req;
    union {
        uint8_t data_lenght;
        uint8_t dlc;
    };
    union {
        uint8_t buffer[MAX_LEN];
        uint8_t data[MAX_LEN];
    };
} CANFRAME;

// ── Hardware version (shared by both platforms) ───────────────────────────────
typedef enum {
    TeslaHW_Unknown = 0,
    TeslaHW_Legacy, // HW1 / HW2 / EAP retrofit — uses 0x3EE / 0x045
    TeslaHW_HW3,
    TeslaHW_HW4,
} TeslaHWVersion;

// ── Operation mode (shared by both platforms) ─────────────────────────────────
// Numbering matches the ESP32's persisted NVS values (ListenOnly=0, Active=1),
// so unifying the enum needs no migration; the ESP32 web UI's op_mode===1==Active
// check also stays valid. Service is Flipper-only (already 2 there). ListenOnly
// is the safe boot default — no TX.
typedef enum {
    OpMode_ListenOnly = 0, // pure passive sniff, no TX at all
    OpMode_Active, // RX + TX, normal operation
    OpMode_Service, // unrestricted, gates aggressive features (Flipper)
} OpMode;
