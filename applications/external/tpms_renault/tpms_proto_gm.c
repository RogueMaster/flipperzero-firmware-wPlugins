#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_gm.c. */

/**
 * General Motors aftermarket sensor. Manchester coded, 130 bits: six
 * bytes of zero preamble and then the payload.
 *
 *     FF FF DD DD II II II II II PP TT CC X
 *
 * - F = flags: bit 5 means the battery is low, and bits 0, 1 and 8 all
 *       clear mean the sensor is in learn mode
 * - D = device type
 * - I = id, 40 bit
 * - P = pressure, 2.75 kPa per step
 * - T = temperature in C, offset -60
 * - C = sum of the nine bytes before it
 *
 * The id is 40 bits wide; the app carries 32, so the top byte is left
 * out. Sensors differ in the lower bytes, so they still tell apart.
 */
bool tpms_decode_gm(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 80) return false;

    uint8_t b[10] = {0};
    tpms_bits_extract(bits, 0, 80, b);

    if((tpms_add_bytes(b, 9) & 0xFF) != b[9]) return false;

    /* An all-zero payload passes a sum checksum trivially. */
    bool all_zero = true;
    for(uint8_t i = 0; i < 9; i++) {
        if(b[i]) {
            all_zero = false;
            break;
        }
    }
    if(all_zero) return false;

    const uint16_t flags = (uint16_t)((b[0] << 8) | b[1]);

    frame->id = ((uint32_t)b[3] << 24) | ((uint32_t)b[4] << 16) | ((uint32_t)b[5] << 8) | b[6];
    frame->pressure_kpa_x100 = (int32_t)b[7] * 275;
    frame->temperature_c = (int16_t)((int16_t)b[8] - 60);
    frame->flags = b[1];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if((flags >> 5) & 1) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
