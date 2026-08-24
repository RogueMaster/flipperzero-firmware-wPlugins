#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_elantra2012.c. */

/**
 * TRW GQ4-44T: FSK, 8 byte Manchester encoded frame with a CRC-8.
 * Seen on Hyundai Elantra and Honda Civic.
 *
 *     PP TT II II II II FF CC
 *
 * - P = pressure in kPa, offset +60
 * - T = temperature in C, offset -50
 * - I = id, 32 bit
 * - F = flags: 0x04 storage mode, 0x02 battery low, 0x01 triggered
 * - C = CRC-8, poly 0x07, init 0x00, over the whole frame
 */
bool tpms_decode_elantra2012(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 64) < 64) return false;
    if(tpms_crc8(b, 8, 0x07, 0x00) != 0) return false;

    frame->id = ((uint32_t)b[2] << 24) | ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 8) | b[5];
    frame->pressure_kpa_x100 = ((int32_t)b[0] + 60) * 100;
    frame->temperature_c = (int16_t)((int16_t)b[1] - 50);
    frame->flags = b[6];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP | TPMS_HAS_BATTERY;
    if(b[6] & 0x02) frame->have |= TPMS_BATTERY_LOW;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
