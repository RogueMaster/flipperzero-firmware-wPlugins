#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_citroen.c. */

/**
 * Citroen: FSK, 10 byte Manchester encoded frame with an XOR checksum.
 * Also Peugeot and likely Fiat, Mitsubishi and other VDO types.
 *
 *     UU IIIIIIII FR PP TT BB CC
 *
 * - U = state, not covered by the checksum
 * - I = id, 32 bit
 * - F = flags
 * - R = repeat counter
 * - P = pressure, 1.364 kPa per step
 * - T = temperature in C, offset -50
 * - B = battery?
 * - C = the byte that makes the XOR of bytes 1 to 9 zero
 *
 * Jeep sensors put the same frame on air with pressure in 2.728 kPa
 * steps, which cannot be told apart from this one — rtl_433 ships its
 * Jeep decoder disabled for that very reason, and frames from those
 * sensors show up here with half the pressure.
 */
bool tpms_decode_citroen(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[11] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 88) < 80) return false;

    /* Neither pressure nor temperature is ever really zero. */
    if(b[6] == 0 || b[7] == 0) return false;
    if(tpms_xor_bytes(&b[1], 9) != 0) return false;

    frame->id = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    frame->pressure_kpa_x100 = ((int32_t)b[6] * 1364 + 5) / 10;
    frame->temperature_c = (int16_t)((int16_t)b[7] - 50);
    frame->flags = (uint8_t)(b[5] >> 4);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, 10);
    frame->raw_len = 10;
    return true;
}
