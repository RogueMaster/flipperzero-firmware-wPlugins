#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_tyreguard400.c. */

/**
 * TyreGuard 400. Manchester coded, 88 bits with a CRC-8 that covers the
 * 28 bit sync word as well — so the sync is put back in front of the
 * payload before the checksum is worked out.
 *
 *     [sync fd 5f d5 f] III IIII PP TT FF CC
 *
 * - I = id, 28 bit
 * - P = pressure in kPa, with three more bits in the flags
 * - T = temperature in C, offset -40
 * - F = flags: leak and peering bits, plus the top bits of the pressure
 * - C = CRC-8, poly 0x31, init 0xdd, over the whole frame
 */
bool tpms_decode_tyreguard400(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 60) return false;

    uint8_t b[11] = {0xFD, 0x5F, 0xD5, 0xF0};
    for(uint16_t i = 0; i < 60; i++) {
        const uint16_t position = (uint16_t)(28 + i);
        if(tpms_bit_at(bits, i)) {
            b[position >> 3] |= (uint8_t)(0x80 >> (position & 7));
        }
    }

    if(tpms_crc8(b, 11, 0x31, 0xDD) != 0) return false;

    const uint8_t flags = b[9];

    frame->id = ((uint32_t)(b[3] & 0x0F) << 24) | ((uint32_t)b[4] << 16) | ((uint32_t)b[5] << 8) |
                b[6];
    frame->pressure_kpa_x100 = (int32_t)(b[7] | ((flags & 0x70) << 4)) * 100;
    frame->temperature_c = (int16_t)((int16_t)b[8] - 40);
    frame->flags = flags;
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
