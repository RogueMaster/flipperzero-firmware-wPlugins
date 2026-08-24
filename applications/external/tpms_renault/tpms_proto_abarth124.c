#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_abarth124.c, which covers two
 * sensors sharing one frame format. */

/**
 * Common layout, Manchester encoded after the preamble:
 *
 *     II II II II ?? PP TT SS CC [FF CR CR]
 *
 * - I = id, 32 bit
 * - ? = unknown, changes with status
 * - P = pressure: 1.38 kPa per step on the TG1C, 3 kPa on the Q85
 * - T = temperature in C: offset -50 on the TG1C, -55 on the Q85
 * - S = status
 * - C = XOR of bytes 0 to 8, which must come out zero
 * - F, CR = only on the Q85: a fixed byte and a CRC-16 CCITT-FALSE over
 *   bytes 0 to 9, little-endian
 */
static bool tpms_abarth124_common(
    const uint8_t* bits,
    uint16_t nbits,
    uint16_t data_bits,
    TpmsFrame* frame) {
    uint8_t b[12] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, data_bits) < data_bits) return false;
    if(tpms_xor_bytes(b, 9) != 0) return false;

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->flags = b[7];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    const uint16_t length = (uint16_t)(data_bits / 8);
    memcpy(frame->raw, b, length);
    frame->raw_len = (uint8_t)length;
    return true;
}

/** VDO TG1C: Abarth 124 Spider, some Fiat 124 Spider, some Mazda MX-5. */
bool tpms_decode_abarth124(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(!tpms_abarth124_common(bits, nbits, 72, frame)) return false;

    frame->pressure_kpa_x100 = (int32_t)frame->raw[5] * 138;
    frame->temperature_c = (int16_t)((int16_t)frame->raw[6] - 50);

    /* The XOR checksum alone is weak and this preamble is shared with
     * other protocols, so the sensor's own working range is used as a
     * second, independent filter — as rtl_433 does here. */
    if(frame->temperature_c < -50 || frame->temperature_c > 125) return false;
    return true;
}

/** Shenzhen EGQ Q85: the same frame with two more bytes and a CRC-16. */
bool tpms_decode_egq_q85(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(!tpms_abarth124_common(bits, nbits, 96, frame)) return false;

    const uint16_t crc_frame = (uint16_t)((frame->raw[11] << 8) | frame->raw[10]);
    if(tpms_crc16(frame->raw, 10, 0x1021, 0xFFFF) != crc_frame) return false;

    frame->pressure_kpa_x100 = (int32_t)frame->raw[5] * 300;
    frame->temperature_c = (int16_t)((int16_t)frame->raw[6] - 55);

    if(frame->temperature_c < -20 || frame->temperature_c > 80) return false;
    return true;
}
