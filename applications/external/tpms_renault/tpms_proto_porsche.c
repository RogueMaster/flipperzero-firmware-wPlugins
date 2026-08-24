#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_porsche.c. */

/**
 * Porsche Boxster and Cayman: FSK, 10 byte differentially Manchester
 * encoded frame with a CRC-16.
 *
 *     II II II II PP TT FF FF CC CC
 *
 * - I = id, 32 bit
 * - P = pressure, 2.5 kPa per step, offset -100 kPa
 * - T = temperature in C, offset -40
 * - F = flags
 * - C = CRC-16, poly 0x1021, init 0xffff, over the whole frame
 */
bool tpms_decode_porsche(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[10] = {0};
    uint16_t decoded = 0;
    tpms_diff_manchester_decode(bits, 0, nbits, b, 80, &decoded);
    if(decoded < 80) return false;
    if(tpms_crc16(b, 10, 0x1021, 0xFFFF) != 0) return false;

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = (int32_t)b[4] * 250 - 10000;
    frame->temperature_c = (int16_t)((int16_t)b[5] - 40);
    frame->flags = b[6];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
