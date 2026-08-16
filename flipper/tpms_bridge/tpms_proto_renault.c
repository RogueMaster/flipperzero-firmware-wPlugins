#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_renault.c and
 * src/devices/tpms_renault_0435r.c. */

/**
 * FSK, 9 byte Manchester encoded frame with a CRC.
 * Seen on Renault Clio, Captur, Zoe and maybe Dacia Sandero.
 *
 *     F F/P PP TT II II II ?? ?? CC
 *
 * - F = flags
 * - P = pressure, 10 bit, 0.75 kPa per step
 * - T = temperature in C, offset -30
 * - I = id, 24 bit little-endian
 * - ? = unknown, mostly 0xffff
 * - C = CRC-8, poly 0x07, init 0x00, over the first 8 bytes
 */
bool tpms_decode_renault(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[9] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 72) < 72) return false;
    if(tpms_crc8(b, 8, 0x07, 0x00) != b[8]) return false;

    const uint16_t pressure_raw = (uint16_t)(((b[0] & 0x03) << 8) | b[1]);

    frame->id = ((uint32_t)b[5] << 16) | ((uint32_t)b[4] << 8) | b[3];
    frame->pressure_kpa_x100 = (int32_t)pressure_raw * 75;
    frame->temperature_c = (int16_t)((int16_t)b[2] - 30);
    frame->flags = (uint8_t)(b[0] >> 2);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Renault 0435R: the same 9 byte Manchester frame, but with an XOR
 * checksum and a different field layout.
 *
 *     II II II FF PP TT AA TT CC
 *
 * - I = id, 24 bit
 * - F = flags, observed to be 0xc0 always
 * - P = pressure, 1/0.75 kPa per step
 * - T = temperature in C, offset -50
 * - A = centrifugal acceleration, 5 m/s2 per step
 * - C = the byte that makes the XOR of all nine bytes zero, plus a
 *       transmission counter in its low bits
 */
bool tpms_decode_renault_0435r(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[9] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 72) < 72) return false;
    if(tpms_xor_bytes(b, 9) != 0) return false;

    /* The counter runs 0..30 while the top bit is set, then the top bit
     * drops and the counter stays at zero. Anything else is not a frame
     * of this protocol — worth checking, since an XOR checksum on its own
     * lets a lot through. */
    const uint8_t tick = (uint8_t)(b[8] & 0x7F);
    const uint8_t has_tick = (uint8_t)(b[8] >> 7);
    if(b[8] && (!has_tick || tick > 30)) return false;

    frame->id = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
    frame->pressure_kpa_x100 = ((int32_t)b[4] * 400 + 1) / 3; /* raw / 0.75 kPa */
    frame->temperature_c = (int16_t)((int16_t)b[5] - 50);
    frame->flags = b[3];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
