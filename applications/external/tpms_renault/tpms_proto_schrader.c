#include "tpms_proto.h"

#include <string.h>

/* Ported from rtl_433 src/devices/schraeder.c, which holds several
 * Schrader sensors that share little beyond the manufacturer. */

/**
 * Schrader Electronics, the plain OOK sensor. Manchester coded, 68 bits
 * with the first four a sync of no fixed value — which is why this one is
 * looked for at the start of a burst rather than behind a sync word.
 *
 *     FF II II II II PP TT CC
 *
 * - F = flags
 * - I = id, 28 bit
 * - P = pressure, 25 mbar per step
 * - T = temperature in C, offset -50
 * - C = CRC-8, poly 0x07, init 0xf0, over the first seven bytes
 */
bool tpms_decode_schrader(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 67) return false;

    uint8_t b[8] = {0};
    /* The row rtl_433 works on opens with a synthetic zero bit, so its
     * four sync bits are three real ones here. */
    tpms_bits_extract(bits, 3, 64, b);

    if(b[7] != tpms_crc8(b, 7, 0x07, 0xF0)) return false;

    frame->id = ((uint32_t)(b[1] & 0x0F) << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) |
                b[4];
    frame->pressure_kpa_x100 = (int32_t)b[5] * 250; /* 25 mbar a step */
    frame->temperature_c = (int16_t)((int16_t)b[6] - 50);
    frame->flags = (uint8_t)(((b[0] & 0x0F) << 4) | (b[1] >> 4));
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Schrader EG53MA4, also the Opel, Saab, Vauxhall and Chevrolet OEM
 * sensor. Forty sync bits of no fixed value, then ten bytes.
 *
 *     ?? ?? ?? ?? II II II PP TT CC
 *
 * - ? = preamble, status and battery flags
 * - I = id, 24 bit
 * - P = pressure, 25 mbar per step
 * - T = temperature in degrees Fahrenheit
 * - C = sum of the nine bytes before it
 */
bool tpms_decode_schrader_eg53ma4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 119) return false;

    uint8_t b[10] = {0};
    tpms_bits_extract(bits, 39, 80, b);

    /* Everything zero passes a sum checksum, so it has to go. */
    if(!b[1] && !b[2] && !b[4] && !b[5] && !b[7] && !b[8]) return false;
    if((tpms_add_bytes(b, 9) & 0xFF) != b[9]) return false;

    frame->id = ((uint32_t)b[4] << 16) | ((uint32_t)b[5] << 8) | b[6];
    frame->pressure_kpa_x100 = (int32_t)b[7] * 250;
    /* The only one of these sensors to report Fahrenheit. */
    frame->temperature_c = (int16_t)(((int32_t)b[8] - 32) * 5 / 9);
    frame->flags = b[3];
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Schrader SMD3MA4, fitted to Subaru. OOK, 36 bits of preamble and then
 * 38 Manchester encoded bits with a two bit checksum. No temperature.
 *
 *     1 FFF IIIIIIIIIIIIIIIIIIIIIIII PPPPPPPP XX
 *
 * - F = flags: 0 learning, 3 pressure change, 5 wake-up, 7 driving
 * - I = id, 24 bit
 * - P = pressure, 0.2 PSI per step
 * - X = two bit checksum: the two bit groups of the payload, added
 *       together, always come to one
 *
 * The Schrader 3039 for Infiniti, Nissan and Renault puts the same frame
 * on air with 0.25 PSI per step. Nothing distinguishes the two, so those
 * sensors read a fifth low here; rtl_433 ships both interpretations and
 * warns that a single transmission then shows up twice.
 */
bool tpms_decode_schrader_smd3ma4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    uint8_t b[6] = {0};
    if(tpms_manchester_decode(bits, 0, nbits, b, 38) < 38) return false;
    /* Only the 38 bits that are really there: the checksum adds up bit
     * groups across whole bytes, so the two spare bits must stay zero. */
    tpms_bits_invert_partial(b, 38);

    if(!b[0] && !b[1] && !b[2] && !b[3]) return false;

    uint32_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        sum += ((b[i] >> 0) & 0x03) + ((b[i] >> 2) & 0x03) + ((b[i] >> 4) & 0x03) +
               ((b[i] >> 6) & 0x03);
    }
    if((sum & 0x03) != 1) return false;

    const int32_t pressure = (int32_t)(((b[3] & 0x0F) << 4) | (b[4] >> 4));

    frame->id = ((uint32_t)(b[0] & 0x0F) << 20) | ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) |
                (uint32_t)(b[3] >> 4);
    frame->pressure_kpa_x100 = tpms_kpa_from_psi_x100(pressure * 20);
    frame->flags = (uint8_t)((b[0] & 0x70) >> 4);
    frame->have = TPMS_HAS_PRESSURE;

    memcpy(frame->raw, b, 5);
    frame->raw_len = 5;
    return true;
}

/**
 * Schrader MRXBC5A4, the BMW sensor. Manchester coded, 61 bits: a fixed
 * sixteen bit wake and sync, then the payload.
 *
 *     FFF IIIIIIIIIIIIIIIIIIIIIIII PPPPPPPPP CC TTTTTTTT
 *
 * - F = flags, 0b010 means sleep acknowledged
 * - I = id, 24 bit
 * - P = pressure in kPa, 9 bit
 * - C = two bit integrity check over the id, pressure and itself
 * - T = temperature in C, offset -50
 */
bool tpms_decode_schrader_mrxbc5a4(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 45) return false;

    uint8_t b[6] = {0};
    tpms_bits_extract(bits, 0, 45, b);

    const uint32_t id = ((uint32_t)(b[0] & 0x1F) << 19) | ((uint32_t)b[1] << 11) |
                        ((uint32_t)b[2] << 3) | (uint32_t)(b[3] >> 5);
    if(id == 0 || id == 0xFFFFFF) return false;

    /* The check is (ones at even positions + 2 * ones) - 1, modulo four,
     * over the 35 bits holding the id, the pressure and itself. */
    uint16_t even_ones = 0;
    uint16_t ones = 0;
    for(uint16_t i = 3; i < 38; i++) {
        if(tpms_bit_at(b, i)) {
            ones++;
            if(((i - 3) % 2) == 0) even_ones++;
        }
    }
    const uint8_t check = (uint8_t)((even_ones + 2 * ones - 1) & 0x03);
    const uint8_t frame_check = (uint8_t)(((b[4] >> 3) & 1) << 1 | ((b[4] >> 2) & 1));
    if(check != frame_check) return false;

    const int32_t pressure = (int32_t)(((b[3] & 0x1F) << 4) | (b[4] >> 4));
    const int16_t temperature = (int16_t)((((b[4] & 0x03) << 5) | (b[5] >> 3)) - 50);

    /* Two bits of checksum let three quarters of a corrupt payload
     * through, so the sensor's own working range is used as a second
     * filter, as rtl_433 does here. */
    if(pressure > 450 || temperature < -40 || temperature > 85) return false;

    frame->id = id;
    frame->pressure_kpa_x100 = pressure * 100;
    frame->temperature_c = temperature;
    frame->flags = (uint8_t)((b[0] >> 5) & 0x07);
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}

/**
 * Schrader motorcycle sensor. Manchester coded, thirteen bits of preamble
 * and then seven bytes with a CRC-8.
 *
 *     II II II IP PP TT CC
 *
 * - I = id, 24 bit
 * - P = pressure, 0.5 kPa per step, 10 bit
 * - T = temperature in C, offset -50
 * - C = CRC-8, poly 0x07, init 0xe0, over the whole frame
 */
bool tpms_decode_schrader_motorcycle(const uint8_t* bits, uint16_t nbits, TpmsFrame* frame) {
    if(nbits < 56) return false;

    uint8_t b[7] = {0};
    tpms_bits_extract(bits, 0, 56, b);

    if(tpms_crc8(b, 7, 0x07, 0xE0) != 0) return false;

    frame->id = ((uint32_t)(b[0] & 0x03) << 22) | ((uint32_t)b[1] << 14) | ((uint32_t)b[2] << 6) |
                (uint32_t)(b[3] >> 2);
    frame->pressure_kpa_x100 = (int32_t)(((b[3] & 0x03) << 8) | b[4]) * 50;
    frame->temperature_c = (int16_t)((int16_t)b[5] - 50);
    frame->flags = 0;
    frame->have = TPMS_HAS_PRESSURE | TPMS_HAS_TEMP;

    memcpy(frame->raw, b, sizeof(b));
    frame->raw_len = sizeof(b);
    return true;
}
