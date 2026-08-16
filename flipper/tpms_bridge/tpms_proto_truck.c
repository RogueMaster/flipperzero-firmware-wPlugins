#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_truck.c. */

/**
 * Unbranded solar TPMS for trucks: FSK, Manchester encoded, with an XOR
 * checksum. The payload starts four bits into the decoded stream.
 *
 *     II II II II WW FP PP TT CC
 *
 * - I = id, 32 bit
 * - W = wheel number
 * - F = flags: 0x4 pressure alert, 0x3 battery ok, 0x0 battery low
 * - P = pressure in kPa, 12 bit
 * - T = temperature in C
 * - C = the byte that makes the XOR of all nine bytes zero
 */
bool tpms_decode_truck(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t decoded[10] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, decoded, 76) < 76) return false;

    uint8_t b[9] = {0};
    tpms_bits_extract(decoded, 4, 72, b);

    /* An all-zero id is what noise decodes to, and the XOR checksum
     * cannot tell it apart from a frame. */
    if(!b[0] && !b[1] && !b[2] && !b[3]) return false;
    if(tpms_xor_bytes(b, 9) != 0) return false;

    const uint8_t flags = (uint8_t)(b[5] >> 4);

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = (int32_t)(((b[5] & 0x0F) << 8) | b[6]) * 100;
    frame->temperature_c = (int16_t)b[7];
    frame->flags = flags;
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if((flags & 0x03) != 0x03) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
