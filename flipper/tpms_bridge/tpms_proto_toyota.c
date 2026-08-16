#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_toyota.c. */

/**
 * Toyota: FSK, 9 byte differentially Manchester encoded frame with a
 * CRC-8. Pacific Industries PMV-C210, seen on a Toyota Auris (Corolla);
 * built by Pacific Industrial and sometimes TRW Automotive, so other
 * brands may use it too.
 *
 * Sync is 14 bits, then 72 differentially Manchester encoded bits.
 *
 *     II II II II SP PT TS PP CC
 *
 * - I = id, 32 bit
 * - S = status bits
 * - P = pressure, a quarter PSI per step, offset -7 PSI; repeated
 *       inverted in byte 7 as a check
 * - T = temperature in C, offset -40
 * - C = CRC-8, poly 0x07, init 0x80
 */
bool tpms_decode_toyota(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[10] = {0};
    uint16_t decoded = 0;
    const uint16_t consumed = tpms_diff_manchester_decode(bits, 0, nbits, b, 80, &decoded);

    if(consumed < 144 || decoded < 72) return false;
    if(tpms_crc8(b, 8, 0x07, 0x80) != b[8]) return false;

    const uint16_t pressure = (uint16_t)(((b[4] & 0x7F) << 1) | (b[5] >> 7));
    const uint16_t pressure_check = (uint16_t)(b[7] ^ 0xFF);
    if(pressure != pressure_check) return false;

    const uint16_t temperature = (uint16_t)(((b[5] & 0x7F) << 1) | (b[6] >> 7));

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100((int32_t)pressure * 25 - 700);
    frame->temperature_c = (int16_t)((int16_t)temperature - 40);
    frame->flags = (uint8_t)((b[4] & 0x80) | (b[6] & 0x7F));
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, 9);
    frame->raw_len = 9;
    return true;
}
