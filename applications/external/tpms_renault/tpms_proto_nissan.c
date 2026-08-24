#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_nissan.c. */

/** Nissan's checksum: pairs of bits added up, expected to come out zero. */
static uint8_t tpms_nissan_checksum(const uint8_t* b) {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 4; i++) {
        sum = (uint8_t)(sum + (b[i] >> 7));
        sum = (uint8_t)(sum + (b[i] >> 5));
        sum = (uint8_t)(sum + (b[i] >> 3));
        sum = (uint8_t)(sum + (b[i] >> 1));
        sum = (uint8_t)(sum + (uint8_t)(b[i] << 1));
    }
    sum = (uint8_t)(sum + (b[4] >> 7));
    sum = (uint8_t)(sum + (b[4] >> 5));
    sum = (uint8_t)(sum + (b[4] >> 3));
    return (uint8_t)(~sum & 0x03);
}

/**
 * Nissan: FSK, 37 Manchester encoded bits, inverted, with a two bit
 * checksum.
 *
 *     mode 3 bit, id 24 bit, pressure 8 bit, 2 unknown bits
 *
 * Pressure is a quarter PSI per step, offset -3 PSI.
 */
bool tpms_decode_nissan(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[6] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 40) < 37) return false;
    tpms_bits_invert(b, sizeof(b));

    if(tpms_nissan_checksum(b) != 0) return false;

    const int32_t pressure_raw = (int32_t)(((b[3] & 0x1F) << 3) | (b[4] >> 5));

    frame->id = (uint32_t)(((uint32_t)(b[0] & 0x1F) << 19) | ((uint32_t)b[1] << 11) |
                           ((uint32_t)b[2] << 3) | (uint32_t)(b[3] >> 5));
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100(pressure_raw * 25 - 300);
    frame->flags = (uint8_t)(b[0] >> 5);
    frame->have = TPMS_HAS_PRESSURE;

    memcpy(frame->raw, b, 5);
    frame->raw_len = 5;
    return true;
}
