#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_honda.c. */

/**
 * Honda TRW PPA-GF33: FSK, 8 byte Manchester encoded frame with a CRC-8.
 * Seen on a 2010 Insight, likely shared with Honda models of 2009 to
 * 2016. A different format from the Chrysler TRW sensors.
 *
 * The frame opens with 23 raw bits ending on a deliberate Manchester
 * violation — a desync marker, used here as the sync word.
 *
 *     PP TT II II II II FF CC
 *
 * - P = pressure, 0.2 PSI per step
 * - T = temperature in C, offset -50
 * - I = id, 32 bit
 * - F = flags, 0xe1 while parked in every sample so far
 * - C = CRC-8, poly 0x07, init 0x00, over the first seven bytes
 */
bool tpms_decode_honda(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 64) < 64) return false;
    if(tpms_crc8(b, 7, 0x07, 0x00) != b[7]) return false;

    /* TRW GQ4-44T frames can fit this structure, but land here as a
     * nonsensical pressure below 10 PSI. */
    if(b[0] > 0 && b[0] < 50) return false;

    frame->id = ((uint32_t)b[2] << 24) | ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 8) | b[5];
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100((int32_t)b[0] * 20);
    frame->temperature_c = (int16_t)((int16_t)b[1] - 50);
    frame->flags = b[6];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
