#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_airpuxem.c. */

/**
 * Airpuxem TYH11_EU6_ZQ: FSK, Manchester encoded, with a CRC-8. The
 * payload starts four bits into the decoded stream, behind a constant
 * 0x5 nibble.
 *
 *     II II II II FP PP TT BB
 *
 * - I = id, 32 bit
 * - F = flags and wheel position, and two more bits of the pressure
 * - P = pressure in kPa, 10 bit, offset -100
 * - T = temperature in C, signed
 * - B = battery, 0.02 V per step
 * - CRC-8, poly 0x2f, init 0xaa, over the 64 bits after the header
 */
bool tpms_decode_airpuxem(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t decoded[12] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, decoded, 88) < 84) return false;

    if((decoded[0] >> 4) != 0x5) return false;

    uint8_t payload[10] = {0};
    tpms_bits_extract(decoded, 4, 80, payload);

    uint8_t crc_frame[2] = {0};
    tpms_bits_extract(decoded, 4 + 64, 16, crc_frame);
    if(tpms_crc8(payload, 8, 0x2F, 0xAA) != crc_frame[0]) return false;

    const int32_t pressure =
        (int32_t)(payload[5] | (((payload[4] >> 7) & 1) << 8) | (((payload[4] >> 3) & 1) << 9)) -
        100;

    frame->id = ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
                ((uint32_t)payload[2] << 8) | payload[3];
    frame->pressure_kpa_x100 = pressure * 100;
    frame->temperature_c = (int16_t)(int8_t)payload[6];
    frame->flags = (uint8_t)((payload[4] >> 4) & 0x07);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, payload, 9);
    frame->raw_len = 9;
    return true;
}
