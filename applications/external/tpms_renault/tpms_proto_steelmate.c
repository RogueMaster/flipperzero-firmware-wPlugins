#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/steelmate.c. */

/**
 * Steelmate: FSK, Manchester coded on the wire, inverted, and with the
 * bits of every byte the other way round.
 *
 *     [preamble 0x00 0x00 0x01] II II PP TT BB CC
 *
 * - I = id, 16 bit
 * - P = pressure, 3.125 kPa per step
 * - T = temperature in C, offset -50
 * - B = battery: 3.9 V less 10 mV per step; 0xfe and 0xff are leak alarms
 * - C = sum of the last preamble byte and the five before it
 */
bool tpms_decode_steelmate(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 48) return false;

    uint8_t b[6] = {0};
    tpms_bits_extract(bits, 0, 48, b);
    for(uint8_t i = 0; i < sizeof(b); i++)
        b[i] = tpms_reverse8(b[i]);

    /* The checksum covers the last byte of the preamble as well, which is
     * always 0x01 once turned round. */
    const uint32_t sum = 0x01 + b[0] + b[1] + b[2] + b[3] + b[4];
    if((sum & 0xFF) != b[5]) return false;

    frame->id = ((uint32_t)b[0] << 8) | b[1];
    frame->pressure_kpa_x100 = (int32_t)b[2] * 625 / 2;
    frame->temperature_c = (int16_t)((int16_t)b[3] - 50);
    frame->flags = b[4];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    /* Below about 3.0 V the sensor is on its way out; 0xfe and 0xff are
     * not voltages at all but leak alarms. */
    if(b[4] >= 0xFE || b[4] > 90) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
