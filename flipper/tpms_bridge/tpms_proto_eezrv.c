#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_eezrv.c. */

/**
 * EezTire E618, Carchet and TST-507 sensors. Manchester coded, a preamble
 * of 0xffff and then eight bytes.
 *
 *     CC II II II PP TT F1 F2
 *
 * - C = sum of the seven bytes after it, with the top bit forced on if
 *       the sum overflowed
 * - I = id, 24 bit
 * - P = pressure, 2.5 kPa per step, with a ninth bit in the flags
 * - T = temperature in C, offset -50
 * - F = flags: 0x80 battery low, 0x10 fast leak, 0x20 inflating
 */
bool tpms_decode_eezrv(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 64) return false;

    uint8_t checksum = 0;
    uint8_t b[7] = {0};
    tpms_bits_extract(bits, 0, 8, &checksum);
    tpms_bits_extract(bits, 8, 56, b);

    uint32_t sum = tpms_add_bytes(b, sizeof(b));
    if(sum > 0xFF) sum |= 0x80;
    if((sum & 0xFF) != checksum) return false;

    const uint8_t flags1 = b[5];
    const uint8_t flags2 = b[6];

    frame->id = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
    frame->pressure_kpa_x100 = (int32_t)(((flags2 & 0x01) << 8) | b[3]) * 250;
    frame->temperature_c = (int16_t)((int16_t)b[4] - 50);
    frame->flags = flags1;
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if(flags1 & 0x80) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
