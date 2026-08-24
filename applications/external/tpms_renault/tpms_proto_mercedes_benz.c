#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_mercedes_benz.c. */

/**
 * Mercedes Benz Sprinter 4500: FSK, Manchester coded on the wire, so the
 * payload is read straight out of the sliced bits.
 *
 *     HH II II II II PP TT CF F2 CC
 *
 * - H = header, 0x83 or 0xa3
 * - I = id, 32 bit
 * - P = pressure, 1/2.75 PSI per step
 * - T = temperature in C, offset -51
 * - C = counter in the low five bits, flags in the top three
 * - CC = CRC-8, poly 0x2f, init 0xaa, over the whole frame
 */
bool tpms_decode_mercedes_benz(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 80) return false;

    uint8_t b[10] = {0};
    tpms_bits_extract(bits, 0, 80, b);

    if(tpms_crc8(b, 10, 0x2F, 0xAA) != 0) return false;
    if(b[0] != 0x83 && b[0] != 0xA3) return false;

    frame->id = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100((int32_t)b[5] * 400 / 11);
    frame->temperature_c = (int16_t)((int16_t)b[6] - 51);
    frame->flags = (uint8_t)(b[7] >> 5);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
