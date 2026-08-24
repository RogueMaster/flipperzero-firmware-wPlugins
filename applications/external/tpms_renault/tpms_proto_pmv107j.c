#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_pmv107j.c. */

/**
 * Pacific Industries PMV-107J, seen on Toyota: FSK at half the usual chip
 * rate, 67 differentially Manchester encoded bits with a CRC-8. The whole
 * preamble is seven bits, which is short enough that the checksum does
 * most of the work of telling a frame from noise.
 *
 * The payload is realigned by six bits before it makes sense:
 *
 *     II II II II SP PP TT CC
 *
 * - I = id, 32 bit
 * - S = status: battery low, a counter, a rapid change flag, a fail flag
 * - P = pressure, 2.48 kPa per step, offset -40 steps; repeated inverted
 *       as a check
 * - T = temperature in C, offset -40
 * - C = CRC-8, poly 0x13, init 0x00
 */
bool tpms_decode_pmv107j(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t decoded[10] = {0};
    uint16_t produced = 0;
    const uint16_t consumed = tpms_diff_manchester_decode(bits, 0, nbits, decoded, 70, &produced);

    if(consumed < 67 * 2 || produced < 67) return false;

    uint8_t b[9] = {0};
    b[0] = (uint8_t)(decoded[0] >> 6);
    tpms_bits_extract(decoded, 2, 64, &b[1]);

    if(tpms_crc8(b, 8, 0x13, 0x00) != b[8]) return false;

    const uint8_t pressure = b[5];
    const uint8_t pressure_check = (uint8_t)(b[6] ^ 0xFF);
    if(pressure != pressure_check) return false;
    if(pressure < 40) return false; /* below the offset, so not a reading */

    frame->id = ((uint32_t)b[0] << 26) | ((uint32_t)b[1] << 18) | ((uint32_t)b[2] << 10) |
                ((uint32_t)b[3] << 2) | (uint32_t)(b[4] >> 6);
    frame->pressure_kpa_x100 = ((int32_t)pressure - 40) * 248;
    frame->temperature_c = (int16_t)((int16_t)b[7] - 40);
    frame->flags = (uint8_t)(b[4] & 0x3F);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if(b[4] & 0x20) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
