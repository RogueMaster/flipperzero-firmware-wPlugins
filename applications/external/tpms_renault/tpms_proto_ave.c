#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_ave.c. */

/**
 * AVE: FSK at half the usual chip rate, 8 byte differentially Manchester
 * encoded frame with a CRC-8.
 *
 *     II II II II PP TT MB CC
 *
 * - I = id, 32 bit
 * - P = pressure, in one of four scales chosen by the mode bits
 * - T = temperature in C, offset -50
 * - M = mode in the top two bits, battery in the next three
 * - C = CRC-8, poly 0x31, init 0xff, over the whole frame
 */
bool tpms_decode_ave(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[10] = {0};
    uint16_t decoded = 0;
    tpms_diff_manchester_decode(bits, 0, nbits, b, 80, &decoded);
    if(decoded < 64) return false;
    if(tpms_crc8(b, 8, 0x31, 0xFF) != 0) return false;

    const uint8_t mode = (uint8_t)((b[6] >> 6) & 0x03);
    const uint8_t battery = (uint8_t)((b[6] >> 3) & 0x07);
    const int32_t raw = b[4];

    /* The same reading means four different pressures depending on the
     * mode: two scales, each with and without an offset. */
    int32_t pressure;
    switch(mode) {
    case 0:
        pressure = (raw - 47) * 2352 / 10;
        break;
    case 2:
        pressure = (raw * 10 - 182) * 5491 / 100;
        break;
    case 3:
        pressure = raw * 5491 / 10;
        break;
    case 1:
    default:
        pressure = raw * 2352 / 10;
        break;
    }

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = pressure;
    frame->temperature_c = (int16_t)((int16_t)b[5] - 50);
    frame->flags = (uint8_t)(b[6] & 0x07);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if(battery == 7) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, 8);
    frame->raw_len = 8;
    return true;
}
