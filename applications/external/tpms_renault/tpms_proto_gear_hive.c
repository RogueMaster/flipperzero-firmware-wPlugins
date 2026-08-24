#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_gear_hive.c. */

/**
 * Gear Hive and unbranded aftermarket sensors. Manchester coded, a sync
 * word of 0x2594 and then nine bytes, each XORed with the one before it.
 *
 *     CC CS II II II PP TT KK
 *
 * - C = a 12 bit rolling counter, S = sensor class
 * - I = id, 24 bit
 * - P = pressure, 6.25 kPa per step, from a base the sensor class sets
 * - T = temperature in C, offset +21
 *
 * There is no checksum; two fixed flag fields stand in for one.
 */
bool tpms_decode_gear_hive(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 72) return false;

    uint8_t raw[9] = {0};
    tpms_bits_extract(bits, 0, 72, raw);

    /* Undo the differential XOR, seeded from the low byte of the sync. */
    uint8_t p[9];
    p[0] = (uint8_t)(raw[0] ^ 0x94);
    for(uint8_t i = 1; i < 9; i++)
        p[i] = (uint8_t)(raw[i] ^ raw[i - 1]);

    if((p[6] & 0x3C) != 0x20) return false;
    if((p[7] & 0x3F) != 0x35) return false;

    const uint8_t sensor_class = (uint8_t)(p[1] & 0x0F);
    const uint8_t base = (uint8_t)((80 + sensor_class * 64) & 0xFF);
    const int32_t steps = (int32_t)((p[5] - base + 256) & 0xFF);
    const int16_t temperature = (int16_t)(((p[7] >> 6) | ((p[6] & 0x03) << 2)) + 21);

    frame->id = ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 8) | p[4];
    frame->pressure_kpa_x100 = steps * 625;
    frame->temperature_c = temperature;
    frame->flags = sensor_class;
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, p, sizeof(p));
    frame->raw_len = sizeof(p);
    return true;
}
