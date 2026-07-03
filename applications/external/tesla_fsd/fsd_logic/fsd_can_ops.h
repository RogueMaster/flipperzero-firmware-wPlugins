#pragma once
/*
 * fsd_can_ops.h — stateless Tesla CAN frame primitives shared by both platforms.
 *
 * Like fsd_checksum.h, these take raw byte pointers (not a frame struct) so the
 * one implementation works for the Flipper CANFRAME and the ESP32 CanFrame
 * alike, with no frame-type coupling. They were duplicated in both handlers;
 * keeping one copy removes the drift (e.g. the ESP32 had a china_mode branch in
 * its FSD-selected check that the Flipper copy lacked — folded in here as a
 * parameter, so each platform keeps its own behavior while sharing the logic).
 *
 * Header-only `static inline`: no new build-system source file on either side.
 */

#include <stdbool.h>
#include <stdint.h>

// Set or clear one bit in a 64-bit (8-byte) CAN data field. Out-of-range no-op.
static inline void tesla_set_bit(uint8_t* data, int bit, bool value) {
    if(bit < 0 || bit >= 64) return;
    int byte_idx = bit / 8;
    uint8_t mask = (uint8_t)(1u << (bit % 8));
    if(value)
        data[byte_idx] |= mask;
    else
        data[byte_idx] &= (uint8_t)(~mask);
}

// Multiplexor id: low 3 bits of byte 0.
static inline uint8_t tesla_read_mux(const uint8_t* data) {
    return data[0] & 0x07u;
}

// UI "FSD selected" flag from DAS_autopilotControl byte 4 bit 6.
// force_fsd / china_mode bypass the UI check (china_mode is ESP32-only today;
// Flipper passes false).
static inline bool
    tesla_is_fsd_selected(const uint8_t* data, uint8_t dlc, bool force_fsd, bool china_mode) {
    if(force_fsd || china_mode) return true;
    if(dlc < 5) return false;
    return (data[4] >> 6) & 0x01u;
}
