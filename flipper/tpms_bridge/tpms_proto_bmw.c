#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_bmw.c. */

/**
 * BMW Gen4 and Gen5, and the Audi pressure alert: FSK at twice the usual
 * chip rate, Manchester encoded and inverted, with a CRC-8. Sensors from
 * HUF/Beru, Continental, Schrader/Sensata and Audi all speak it, told
 * apart by the brand byte.
 *
 *     BB II II II II PP TT F1 F2 F3 CC
 *
 * - B = brand: 0x00 Audi alert, 0x03 HUF/Beru, 0x23 Schrader/Sensata,
 *       0x80 Continental, 0x88 Audi
 * - I = id, 32 bit
 * - P = pressure, 2.45 kPa per step
 * - T = temperature in C, offset -52
 * - F = flags, their meaning depending on the brand
 * - C = CRC-8, poly 0x2f, init 0xaa
 *
 * The Audi alert frame is eight bytes instead of eleven.
 */
bool tpms_decode_bmw(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[12] = {0};
    const uint16_t decoded = tpms_manchester_decode(bits, 0, nbits, b, 88);

    uint8_t length = 11;
    if(decoded < 88) {
        if(decoded < 64) return false;
        length = 8; /* the shorter Audi alert */
    }

    tpms_bits_invert(b, length);
    if(tpms_crc8(b, length, 0x2F, 0xAA) != 0) return false;

    frame->id = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    frame->pressure_kpa_x100 = (int32_t)b[5] * 245;
    frame->temperature_c = (int16_t)((int16_t)b[6] - 52);
    frame->flags = b[0]; /* the brand */
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, length);
    frame->raw_len = length;
    return true;
}
