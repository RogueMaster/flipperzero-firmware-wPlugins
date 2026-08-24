#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_sefis_m3.c. */

/**
 * Sefis M3, Careud, Sykik SRTP300: FSK, 9 byte Manchester encoded frame,
 * inverted, with a CRC-16.
 *
 *     B0 B1 B2 B3 B4 B5 B6 CC CC
 *
 * - CC = CRC-16, poly 0x1021, init 0x0000, over the first seven bytes
 * - pressure comes from B4 and B5 through an odd two bit page prefix
 * - temperature in C = 14 + low nibble of (B2 + B5)
 *
 * No field of this frame is a fixed sensor id, so everything decoded here
 * lands in a single row.
 */
bool tpms_decode_sefis_m3(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[9] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 72) < 72) return false;
    tpms_bits_invert(b, sizeof(b));

    const uint16_t crc_frame = (uint16_t)((b[7] << 8) | b[8]);
    if(tpms_crc16(b, 7, 0x1021, 0x0000) != crc_frame) return false;

    frame->id = 0;
    frame->flags = b[6];
    frame->have = TPMS_HAS_TEMP;

    /* The pages are not in order: the top three bits of B4 select one of
     * four, and only four of the eight values mean anything. */
    int16_t page = -1;
    switch(b[4] >> 5) {
    case 7:
        page = 0;
        break;
    case 4:
        page = 1;
        break;
    case 5:
        page = 2;
        break;
    case 2:
        page = 3;
        break;
    default:
        break;
    }

    if(page >= 0) {
        const int32_t code = ((int32_t)page << 13) | ((b[4] & 0x1F) << 8) | b[5];
        int32_t pressure = ((code - 0x0E00) * 1000) / 1024; /* / 102.4 kPa */
        if(pressure < 0) pressure = 0;
        frame->pressure_kpa_x100 = pressure;
        frame->have |= TPMS_HAS_PRESSURE;
    }

    frame->temperature_c = (int16_t)(14 + (((b[2] + b[5]) & 0xFF) & 0x0F));

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
