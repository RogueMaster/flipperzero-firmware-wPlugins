#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_hyundai_vdo.c. */

/**
 * Hyundai VDO: FSK, 10 byte Manchester encoded frame with a CRC-8.
 *
 *     SS II II II II FR PP TT BB CC
 *
 * - S = state
 * - I = id, 32 bit
 * - F = flags
 * - R = repeat counter
 * - P = pressure, 1.375 kPa per step
 * - T = temperature in C, offset -50
 * - B = battery?
 * - C = CRC-8, poly 0x07, init 0xaa, over the first nine bytes
 */
bool tpms_decode_hyundai_vdo(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[10] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 80) < 80) return false;
    if(tpms_crc8(b, 9, 0x07, 0xAA) != b[9]) return false;

    frame->id = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    frame->pressure_kpa_x100 = ((int32_t)b[6] * 1375 + 5) / 10;
    frame->temperature_c = (int16_t)((int16_t)b[7] - 50);
    frame->flags = (uint8_t)(b[5] >> 4);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
