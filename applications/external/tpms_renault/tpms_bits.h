#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Bit and checksum helpers shared by the protocol decoders. They are
 * deliberately the same functions rtl_433 uses (src/bit_util.c,
 * src/bitbuffer.c), with the same names and the same argument order, so
 * that a decoder ported from there can be compared line by line with the
 * original. */

/** Bit at position pos, counted from the most significant bit of byte 0. */
uint8_t tpms_bit_at(const uint8_t* bits, uint16_t pos);

/** Append a bit to a bit array being built. The array must start zeroed. */
void tpms_bit_append(uint8_t* out, uint16_t* nbits, uint8_t bit);

/** Invert every bit of a bit array, in place. */
void tpms_bits_invert(uint8_t* bits, uint16_t nbytes);

/** Invert nbits bits, leaving the spare bits of the last byte alone, as
 * bitbuffer_invert() in rtl_433 does. Decoders that add up bit groups
 * across whole bytes can tell the difference. */
void tpms_bits_invert_partial(uint8_t* bits, uint16_t nbits);

/** Copy nbits bits starting at `start` to the front of `out`, as
 * bitbuffer_extract_bytes() does. `out` must start zeroed. */
void tpms_bits_extract(const uint8_t* in, uint16_t start, uint16_t nbits, uint8_t* out);

/** Manchester decode, IEEE convention, as bitbuffer_manchester_decode().
 *
 * Reads pairs of bits starting at `start` and stops at `len`, at
 * `max_bits` of output, or at the first pair of equal bits — a coding
 * violation, which is where the frame ended. Returns how many bits were
 * produced. `out` must start zeroed.
 */
uint16_t tpms_manchester_decode(
    const uint8_t* in,
    uint16_t start,
    uint16_t len,
    uint8_t* out,
    uint16_t max_bits);

/** Differential Manchester decode, as
 * bitbuffer_differential_manchester_decode(): a transition inside the bit
 * period means zero, none means one. Returns the number of input bits
 * consumed, which is what the callers check against the expected length;
 * the number of bits produced is written to *out_bits.
 */
uint16_t tpms_diff_manchester_decode(
    const uint8_t* in,
    uint16_t start,
    uint16_t len,
    uint8_t* out,
    uint16_t max_bits,
    uint16_t* out_bits);

uint8_t tpms_crc4(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init);
uint8_t tpms_crc7(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init);
uint8_t tpms_crc8(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init);
uint8_t tpms_crc8le(const uint8_t* message, uint16_t bytes, uint8_t poly, uint8_t init);
uint16_t tpms_crc16(const uint8_t* message, uint16_t bytes, uint16_t poly, uint16_t init);
uint16_t tpms_crc16lsb(const uint8_t* message, uint16_t bytes, uint16_t poly, uint16_t init);
uint8_t tpms_xor_bytes(const uint8_t* message, uint16_t bytes);
uint32_t tpms_add_bytes(const uint8_t* message, uint16_t bytes);
uint32_t tpms_add_nibbles(const uint8_t* message, uint16_t bytes);
uint8_t tpms_lfsr_digest8(const uint8_t* message, uint16_t bytes, uint8_t gen, uint8_t key);
uint8_t
    tpms_lfsr_digest8_reflect(const uint8_t* message, uint16_t bytes, uint8_t gen, uint8_t key);
uint8_t tpms_parity8(uint8_t byte);
uint8_t tpms_reverse8(uint8_t x);

/** Pressure conversions into the unit the app carries everywhere,
 * hundredths of a kPa. Kept as integer arithmetic: the firmware printf is
 * not required to handle %f, and rounding stays predictable. */
int32_t tpms_kpa_from_psi_x100(int32_t psi_x100);
int32_t tpms_kpa_from_bar_x100(int32_t bar_x100);
