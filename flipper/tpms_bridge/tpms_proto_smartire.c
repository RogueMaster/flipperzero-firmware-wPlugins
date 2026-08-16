#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_smartire.c. */

/**
 * SmarTire, the Aston Martin Vantage and DB9 sensor. OOK at a slow chip
 * rate, differentially Manchester coded, six bytes with a CRC-7.
 *
 *     VV MI II II FF CC
 *
 * - V = pressure or temperature, depending on the message type
 * - M = message type: 0 pressure, 1 temperature
 * - I = id, 22 bit
 * - F = flags, the top bit meaning a quick inflate was seen
 * - C = CRC-7, poly 0x45, init 0x6f
 *
 * Pressure and temperature travel in separate messages, five of each, so
 * a sensor's row fills in over two transmissions.
 */
bool tpms_decode_smartire(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[7] = {0};
    uint16_t decoded = 0;
    tpms_diff_manchester_decode(bits, 0, nbits, b, 48, &decoded);
    if(decoded < 47) return false; /* the last bit is always missing */

    if(tpms_crc7(b, 6, 0x45, 0x6F) != 0) return false;

    const uint8_t type = (uint8_t)((b[1] & 0xC0) >> 6);
    const int32_t value = (int32_t)b[0] - 40;

    frame->id = ((uint32_t)(b[1] & 0x3F) << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->flags = (uint8_t)(b[4] & 0x7F);

    if(type == 0) {
        frame->pressure_kpa_x100 = value * 250;
        frame->have = TPMS_HAS_PRESSURE;
    } else if(type == 1) {
        frame->temperature_c = (int16_t)value;
        frame->have = TPMS_HAS_TEMP;
    } else {
        return false;
    }

    memcpy(frame->raw, b, 6);
    frame->raw_len = 6;
    return true;
}
