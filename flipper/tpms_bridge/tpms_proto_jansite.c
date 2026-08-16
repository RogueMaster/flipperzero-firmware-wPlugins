#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_jansite.c,
 * src/devices/tpms_jansite_solar.c and src/devices/tpms_jansite_ty588.c.
 * Three aftermarket sensors that share a brand and nothing else. */

/**
 * Jansite TY02S: FSK, 7 byte Manchester encoded frame.
 *
 *     II II II IF PP TT CC
 *
 * - I = id, 28 bit
 * - F = flags
 * - P = pressure, 1.7 kPa per step
 * - T = temperature in C, offset -50
 * - C = a byte whose meaning is unknown
 *
 * The frame carries no checksum anyone has worked out, which is why
 * rtl_433 ships this decoder disabled. Here it runs, but only behind the
 * full 32 bit preamble instead of the 24 bits rtl_433 looks for — with
 * nothing to verify the payload against, the sync word is the only
 * defence against noise.
 */
bool tpms_decode_jansite(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[7] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 56) < 56) return false;

    frame->id = ((uint32_t)b[0] << 20) | ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) |
                (uint32_t)(b[3] >> 4);
    frame->pressure_kpa_x100 = (int32_t)b[4] * 170;
    frame->temperature_c = (int16_t)((int16_t)b[5] - 50);
    frame->flags = (uint8_t)(b[3] & 0x0F);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Jansite Solar: FSK, Manchester encoded and then inverted, with a
 * CRC-16. The sync word already covers the first two decoded bytes
 * (0xdd33), so the capture starts at the id.
 *
 *     II II II FF TT PP ?? CC CC
 *
 * - I = id, 24 bit
 * - F = flags
 * - T = temperature in C, offset -55
 * - P = pressure, 1.6 kPa per step
 * - C = CRC-16, poly 0x8005, init 0x0000, over the seven bytes before it
 */
bool tpms_decode_jansite_solar(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[9] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 72) < 72) return false;
    tpms_bits_invert(b, sizeof(b));

    const uint16_t crc_frame = (uint16_t)((b[7] << 8) | b[8]);
    if(tpms_crc16(b, 7, 0x8005, 0x0000) != crc_frame) return false;

    frame->id = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
    frame->pressure_kpa_x100 = (int32_t)b[5] * 160;
    frame->temperature_c = (int16_t)((int16_t)b[4] - 55);
    frame->flags = b[3];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Jansite TY588-EU2: FSK, 8 byte Manchester encoded frame, a different
 * wire format from the Solar above.
 *
 * - byte 7 repeats byte 0, which is the only integrity check there is
 * - bytes 3 and 4 always add up to 0x30, and the low nibbles of bytes 0
 *   and 1 match: both hold for a genuine frame, so both are checked
 * - temperature in C = (b2 + b5) mod 256 - 139
 * - pressure = ((b5 + b6) mod 256 - 90) * 2.5 kPa
 *
 * No field of this frame is a stable sensor id: what looks like one
 * changes from transmission to transmission. Everything decoded here
 * therefore lands in a single row rather than one row per sensor.
 */
bool tpms_decode_jansite_ty588(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 64) < 64) return false;

    if(b[7] != b[0]) return false;
    if(((b[3] + b[4]) & 0xFF) != 0x30) return false;
    if((b[0] & 0x0F) != (b[1] & 0x0F)) return false;

    const int16_t temperature = (int16_t)(((b[2] + b[5]) & 0xFF) - 139);
    const int16_t pressure_raw = (int16_t)(((b[5] + b[6]) & 0xFF) - 90);
    if(pressure_raw < 0 || temperature < -40 || temperature > 120) return false;

    frame->id = 0;
    frame->pressure_kpa_x100 = (int32_t)pressure_raw * 250;
    frame->temperature_c = temperature;
    frame->flags = b[0];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, 7);
    frame->raw_len = 7;
    return true;
}
