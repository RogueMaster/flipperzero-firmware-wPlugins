#pragma once
/*
 * fsd_checksum.h — Tesla CAN additive checksum, shared by both platforms.
 *
 * Several Tesla frames the app injects (0x399 ISA chime, 0x313 track mode,
 * 0x370 EPAS nag echo, 0x249 SCCM_leftStalk) all protect themselves with the
 * SAME additive checksum: the low and high bytes of the CAN id plus the sum of
 * the covered data bytes, truncated to 8 bits. The car rejects any frame whose
 * checksum doesn't match, so a one-byte drift between the Flipper and ESP32
 * copies of this math is exactly the "the fix doesn't work on my car" failure
 * mode. Keeping it in one header-only function means there is only one
 * implementation for both builds to share, and it is unit-tested on the host
 * (see test/test_fsd_core.c).
 *
 * Header-only `static inline` so neither build system needs a new source file;
 * both fsd_logic/fsd_handler.c (Flipper) and esp32/.firmware/fsd_handler.cpp
 * just include it.
 *
 *   checksum = ( (can_id & 0xFF) + ((can_id >> 8) & 0xFF) + sum(data[0..len-1]) ) & 0xFF
 *
 * Callers pass the covered byte range directly:
 *   ISA/track/nag : tesla_additive_checksum(ID, frame_bytes, 7)  -> goes in byte 7
 *   SCCM left CRC : tesla_additive_checksum(0x249, &bytes[1], 2)  -> goes in byte 0
 */

#include <stdint.h>

static inline uint8_t tesla_additive_checksum(uint32_t can_id, const uint8_t* data, uint8_t len) {
    uint16_t sum = (uint16_t)((can_id & 0xFFu) + ((can_id >> 8) & 0xFFu));
    for(uint8_t i = 0; i < len; i++)
        sum += data[i];
    return (uint8_t)(sum & 0xFFu);
}
