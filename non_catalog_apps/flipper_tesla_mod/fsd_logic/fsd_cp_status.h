#pragma once
/*
 * fsd_cp_status.h — Charge Port status (0x25D CP_status, Vehicle CAN), shared
 * by the Flipper and ESP32 builds.
 *
 * 0x25D is 8 bytes with no counter and no checksum, so a modified copy needs no
 * recomputation. Field layout from the Model 3 DBC (BO_ 605 ID25D CP_status):
 *   CP_type               byte0 bits 0-1   0 US_TESLA, 1 EURO_IEC, 2 GB, 3 IEC_CCS
 *   CP_chargeCablePresent byte0 bit  3
 *   CP_latchState         byte0 bits 5-6   0 SNA, 1 DISENGAGED, 2 ENGAGED, 3 BLOCKING
 *   CP_chargeDoorOpen     byte1 bit  2     (global bit 10)
 *   CP_chargeCableState   byte1 bits 6-7   0 SNA, 1 NOT_CONNECTED, 2 CONNECTED (global bits 14-15)
 *   CP_hvInletExposed     byte7 bit  5     (global bit 61)
 * CP_type reported on a US car is US_TESLA(0); setting it to IEC_CCS(3) is what
 * makes European Superchargers appear in nav / route planning (discussion #228,
 * idea and on-car validation @Tesla234987234sdf).
 *
 * The unrelated 6-byte 0x25D on Party CAN is NOT this frame: every helper here
 * requires DLC 8, so a short 0x25D decodes to nothing.
 *
 * Header-only `static inline` (the ESP32 build compiles no fsd_logic source),
 * so fsd_logic/fsd_handler.c and esp32/.firmware/fsd_handler.cpp share one copy
 * (host-tested by test/test_fsd_core.c and test/test_esp32_core.cpp). Raw-byte
 * pointers, not a frame struct, like fsd_checksum.h / fsd_can_ops.h.
 */

#include <stdbool.h>
#include <stdint.h>

// CP_type enum (DBC VAL_ 605 CP_type).
#define FSD_CP_TYPE_US_TESLA 0u
#define FSD_CP_TYPE_EURO_IEC 1u
#define FSD_CP_TYPE_GB       2u
#define FSD_CP_TYPE_IEC_CCS  3u

// Plug state derived from the plug-related bits in one 0x25D frame. The rewrite
// is allowed only while UNPLUGGED, so UNKNOWN fails closed (no rewrite).
typedef enum {
    FSD_CP_PLUG_UNPLUGGED = 0, // cable state NOT_CONNECTED and no plugged indicator
    FSD_CP_PLUG_PLUGGED = 1, // cable present / connected / latched / door / HV inlet
    FSD_CP_PLUG_UNKNOWN = 2, // cable state SNA with nothing else set
} FsdCpPlugState;

// CP_status runs at about 10 Hz. With none for this long, what the last one
// said is shown as stale.
#define FSD_CP_STATUS_STALE_MS 3000u

// True once a CP_status has been decoded and none has arrived for
// FSD_CP_STATUS_STALE_MS. last_ms is when the last one arrived.
static inline bool fsd_cp_status_stale(bool seen, uint32_t last_ms, uint32_t now_ms) {
    return seen && (uint32_t)(now_ms - last_ms) > FSD_CP_STATUS_STALE_MS;
}

// CP_type the car reports (byte0 bits 0-1).
static inline uint8_t fsd_cp_type_value(uint8_t byte0) {
    return (uint8_t)(byte0 & 0x03u);
}

// byte0 with CP_type forced to IEC_CCS(3); the upper six bits are preserved.
static inline uint8_t fsd_cp_type_make_iec_ccs(uint8_t byte0) {
    return (uint8_t)((byte0 & 0xFCu) | FSD_CP_TYPE_IEC_CCS);
}

// Plug state from the plug-related bits of an 8-byte CP_status frame.
//   PLUGGED  if a cable is present/connected, the latch is engaged/blocking, the
//            charge door is open or the HV inlet is exposed — "cable present or
//            charging in progress" (charging requires the cable present).
//   UNPLUGGED only on an affirmative "not connected": CP_chargeCableState ==
//            NOT_CONNECTED and no plugged indicator set.
//   UNKNOWN  otherwise (CP_chargeCableState SNA with nothing else) -> fail closed.
static inline FsdCpPlugState fsd_cp_plug_state(const uint8_t* data) {
    uint8_t cable_state = (uint8_t)((data[1] >> 6) & 0x03u); // bits 14-15
    uint8_t latch_state = (uint8_t)((data[0] >> 5) & 0x03u); // bits 5-6
    bool cable_present = (data[0] & 0x08u) != 0u; // bit 3
    bool door_open = (data[1] & 0x04u) != 0u; // bit 10
    bool inlet_exposed = (data[7] & 0x20u) != 0u; // bit 61
    bool latched = latch_state == 2u || latch_state == 3u; // ENGAGED / BLOCKING

    if(cable_present || cable_state == 2u || latched || door_open || inlet_exposed)
        return FSD_CP_PLUG_PLUGGED;
    if(cable_state == 1u) // NOT_CONNECTED — the only affirmative "unplugged"
        return FSD_CP_PLUG_UNPLUGGED;
    return FSD_CP_PLUG_UNKNOWN; // SNA / unknown -> fail closed
}

// True when a standard 8-byte CP_status currently reports US_TESLA: the only
// frame the rewrite is eligible to copy (so it can neither echo an already-EU
// frame nor touch a non-US variant). The plug guard is a separate check.
static inline bool fsd_cp_type_rewrite_eligible(const uint8_t* data, uint8_t dlc) {
    return data && dlc == 8u && fsd_cp_type_value(data[0]) == FSD_CP_TYPE_US_TESLA;
}
