#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_ford.c. */

/**
 * Ford: FSK, 8 byte Manchester encoded frame with a sum checksum.
 * Seen on Fiesta, Focus, Kuga, Escape, Transit; on 315 MHz in the United
 * States and on 433.92 MHz elsewhere. Likely VDO sensors built by
 * Continental.
 *
 *     II II II II PP TT FF CC
 *
 * - I = id, 32 bit
 * - P = pressure, PSI * 4; the ninth bit lives in flags bit 0x20
 * - T = temperature in C, offset -56, valid only while bit 0x80 is clear
 * - F = flags: 0x40 moving, 0x08 learn, 0x04 at rest
 * - C = sum of bytes 0 to 6
 */
bool tpms_decode_ford(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 64) < 64) return false;
    if((tpms_add_bytes(b, 7) & 0xFF) != b[7]) return false;

    /* Bit combinations the protocol does not use. rtl_433 rejects them
     * too: the sum checksum is weak and this preamble is shared with
     * several other decoders. */
    uint8_t unknown = 0;
    switch(b[6] & 0x4C) {
    case 0x08: /* learn */
    case 0x04: /* at rest */
    case 0x44: /* moving */
        break;
    default:
        unknown = (uint8_t)(b[6] & 0x4C);
        break;
    }
    unknown |= (uint8_t)(b[6] & 0x90);
    if(unknown != 0) return false;

    const int32_t psi_x4 = (int32_t)(((b[6] & 0x20) << 3) | b[4]);

    frame->id = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100(psi_x4 * 25);
    frame->flags = b[6];
    frame->have = TPMS_HAS_PRESSURE;

    if((b[5] & 0x80) == 0) {
        frame->temperature_c = (int16_t)((int16_t)(b[5] & 0x7F) - 56);
        frame->have |= TPMS_HAS_TEMP;
    }
    if(b[6] & 0x40) frame->have |= TPMS_MOVING;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
