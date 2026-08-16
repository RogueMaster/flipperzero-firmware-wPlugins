#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_trw.c, which covers the same
 * sensor in both of its modulations. */

/**
 * TRW GQ4-70T, fitted to Chrysler cars from 2014 to 2022. Manchester
 * coded on the wire, with a CRC-8. The sensor exists in an OOK and an FSK
 * version, told apart only by the preamble: 0x0001 against 0x7fff.
 *
 *     MM II II II II FN PP TT SS CC X
 *
 * - M = mode or model, 0x5c to 0x5e
 * - I = id, 32 bit
 * - F = status flags, N = sequence number
 * - P = pressure, 0.4 PSI per step
 * - T = temperature in C, offset -50
 * - S = motion status, 0x0e while parked
 * - C = CRC-8, poly 0x07, init 0x00
 */
bool tpms_decode_trw(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 81) return false;

    /* The frame runs out a few bits into the last byte; only the first
     * ten are covered by the checksum and none of the rest is read. */
    uint8_t b[11] = {0};
    tpms_bits_extract(bits, 0, 81, b);

    if(tpms_crc8(b, 10, 0x07, 0x00) != 0) return false;

    frame->id = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) | b[4];
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100((int32_t)b[6] * 40);
    frame->temperature_c = (int16_t)((int16_t)b[7] - 50);
    frame->flags = (uint8_t)(b[5] >> 4);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;
    if(b[8] != 0x0E) frame->have |= TPMS_MOVING;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
