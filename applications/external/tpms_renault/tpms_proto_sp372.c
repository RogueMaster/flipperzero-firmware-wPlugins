#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/tpms_imars_t240.c and
 * src/devices/tpms_jansite_ty468.c, two sensors built around the same
 * SP372 transmitter and sharing a wire format. */

/**
 * Common frame: 32 raw bits of alternating preamble, then 128 Manchester
 * encoded bits making eight bytes, inverted.
 *
 * - byte 7 repeats byte 0
 * - the low nibbles of bytes 0 and 1 match
 * - bytes 3 and 4 add up to a constant that is fixed per sensor unit,
 *   which is the only checksum there is
 */
static bool tpms_sp372_common(const uint8_t* bits, uint16_t nbits, uint8_t* out) {
    if(tpms_manchester_decode(bits, 0, nbits, out, 64) < 64) return false;
    tpms_bits_invert(out, 8);

    if(out[7] != out[0]) return false;
    if((out[0] & 0x0F) != (out[1] & 0x0F)) return false;
    return true;
}

/**
 * Jansite TY-468-eu2 and KKMOON. Two physical units have been calibrated
 * against real readings, each with its own constants; a third would need
 * its own worked out.
 */
bool tpms_decode_jansite_ty468(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(!tpms_sp372_common(bits, nbits, b)) return false;

    int16_t temperature_offset;
    int16_t pressure_offset;
    switch((b[3] + b[4]) & 0xFF) {
    case 0xFB:
        temperature_offset = 224;
        pressure_offset = 273;
        break;
    case 0x64:
        temperature_offset = 153;
        pressure_offset = 201;
        break;
    default:
        return false;
    }

    frame->id = 0; /* no field of this frame is a fixed sensor id */
    frame->temperature_c = (int16_t)(temperature_offset - ((b[2] + b[5]) & 0xFF));
    frame->pressure_kpa_x100 = (int32_t)(pressure_offset - ((b[5] + b[6]) & 0xFF)) * 250;
    frame->flags = b[0];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, 7);
    frame->raw_len = 7;
    return true;
}

/**
 * iMars T240. The same frame, with two other checksum constants — and
 * nobody has worked out where its pressure and temperature live, so all
 * this can say is that such a sensor is transmitting.
 */
bool tpms_decode_imars_t240(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[8] = {0};
    if(!tpms_sp372_common(bits, nbits, b)) return false;

    const uint8_t checksum = (uint8_t)((b[3] + b[4]) & 0xFF);
    if(checksum != 0x41 && checksum != 0x3C) return false;

    frame->id = 0;
    frame->flags = b[0];
    frame->have = 0; /* nothing in this frame has been decoded yet */

    memcpy(frame->raw, b, 7);
    frame->raw_len = 7;
    return true;
}
