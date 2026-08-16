#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_kia.c. */

/**
 * Kia: FSK, Manchester encoded, with a CRC-8. The fields are offset by
 * half a byte throughout.
 *
 * - unknown nibble
 * - pressure, 0.2 PSI per step
 * - temperature in C, offset -50
 * - id, 32 bit
 * - two unknown bytes
 * - CRC-8, poly 0x07, init 0x76, over the first eight bytes; only the
 *   top five bits of the last byte belong to the frame
 */
bool tpms_decode_kia(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[18] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 138) < 138) return false;

    const uint8_t crc_frame = (uint8_t)(b[8] & ~0x07);
    if(tpms_crc8(b, 8, 0x07, 0x76) != crc_frame) return false;

    const uint8_t pressure = (uint8_t)((b[0] << 4) | (b[1] >> 4));
    const uint8_t temperature = (uint8_t)((b[1] << 4) | (b[2] >> 4));

    frame->id = ((uint32_t)b[2] << 28) | ((uint32_t)b[3] << 20) | ((uint32_t)b[4] << 12) |
                ((uint32_t)b[5] << 4) | (uint32_t)(b[6] >> 4);
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100((int32_t)pressure * 20);
    frame->temperature_c = (int16_t)((int16_t)temperature - 50);
    frame->flags = (uint8_t)(b[0] >> 4);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, 9);
    frame->raw_len = 9;
    return true;
}
