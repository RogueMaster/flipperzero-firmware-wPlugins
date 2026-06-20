#pragma once
/*
 * EN 13757-4 §6.2.1.1 Three-out-of-Six (3-of-6) decoder.
 *
 * In T-mode, every nibble of payload data is sent as a 6-bit chip with
 * exactly three '1's, providing some run-length limitation and DC-balance.
 * Two consecutive 6-bit chips encode one byte (high nibble first).
 *
 * 12 chip bits = 1.5 bytes of chip data per 1 byte of payload, so the
 * over-the-air rate is 100 kchip/s -> 66.67 kbit/s effective payload.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Decode a single 6-bit chip into a 4-bit nibble.
 * Returns 0..15 on success, or 0xFF if the chip is not a valid 3-of-6 code. */
uint8_t wmbus_3of6_decode_chip(uint8_t chip6);

/* Decode a stream of chip bits (MSB-first, packed into bytes) into output
 * bytes. `chip_bits` is the number of valid bits in `chips`.
 * Returns number of decoded bytes; sets *err to true if any chip was invalid.
 *
 * Output buffer must be at least chip_bits / 12 bytes. */
size_t wmbus_3of6_decode(
    const uint8_t* chips,
    size_t chip_bits,
    uint8_t* out,
    size_t out_capacity,
    bool* err);
