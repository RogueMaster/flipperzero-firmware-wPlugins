#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_bmw_g3.c. */

/**
 * BMW Gen2 and Gen3: FSK, differentially Manchester encoded, with a
 * CRC-16. Gen2 frames are 10 bytes, Gen3 frames 11.
 *
 *     II II II II PP TT F1 F2 F3 CC CC
 *
 * - I = id, 32 bit
 * - P = pressure, 2.5 kPa per step, offset -43 steps
 * - T = temperature in C, offset -40
 * - F = flags
 * - C = CRC-16, poly 0x1021, init 0x0000, over the whole frame
 */
bool tpms_decode_bmw_g3(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[12] = {0};
    uint16_t decoded = 0;
    tpms_diff_manchester_decode(bits, 0, nbits, b, 88, &decoded);
    if(decoded < 80) return false;

    /* Gen3 carries one byte more. Which one this is shows in the length
     * on the air, which we do not have here, so both are tried. */
    uint8_t length = 11;
    if(decoded < 88 || tpms_crc16(b, 11, 0x1021, 0x0000) != 0) {
        length = 10;
        if(tpms_crc16(b, 10, 0x1021, 0x0000) != 0) return false;
    }

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = ((int32_t)b[4] - 43) * 250;
    frame->temperature_c = (int16_t)((int16_t)b[5] - 40);
    frame->flags = b[7];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, length);
    frame->raw_len = length;
    return true;
}
