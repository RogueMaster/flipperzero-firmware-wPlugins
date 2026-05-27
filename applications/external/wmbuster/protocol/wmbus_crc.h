#pragma once
/*
 * CRC-16/EN-13757
 *   poly  : 0x3D65
 *   init  : 0x0000
 *   refin : false
 *   refout: false
 *   xorout: 0xFFFF
 *
 * Used to protect each 16-byte block on wM-Bus Format A frames.
 * (Block 1 = 10 bytes data + 2 CRC; subsequent blocks = up to 16 + 2 CRC.)
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

uint16_t wmbus_crc(const uint8_t* data, size_t len);

/* Verify Format A frame: input is the L-byte followed by the rest of the
 * packet (still containing per-block CRCs). Returns true if every block CRC
 * matches. On success, writes the stripped payload (no CRCs) into `out`
 * starting from L-byte and returns the new length via *out_len.
 *
 * Format B is verified separately because its CRC layout differs. */
bool wmbus_crc_verify_format_a(
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* out,
    size_t out_capacity,
    size_t* out_len);

/* Format B: single CRC over the whole telegram (excluding the CRC bytes
 * themselves). Last 2 bytes of `frame` are the CRC. */
bool wmbus_crc_verify_format_b(
    const uint8_t* frame,
    size_t frame_len);
