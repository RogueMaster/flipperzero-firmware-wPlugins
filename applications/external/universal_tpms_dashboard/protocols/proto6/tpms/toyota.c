/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Toyota tires TPMS. Usually 443.92 Mhz FSK (In Europe).
 *
 * Preamble + sync + 64 bits of data. ~48us short pulse length.
 *
 * The preamble + sync is something like:
 *
 *   10101010101 (preamble) + 001111[1] (sync)
 *
 * Note: the final [1] means that sometimes it is four 1s, sometimes
 * five, depending on the short pulse length detection and the exact
 * duration of the high long pulse. After the sync, a differential
 * Manchester encoded payload follows. However the Flipper's CC1101
 * often can't decode correctly the initial alternating pattern 101010101,
 * so what we do is to seek just the sync, that is "001111" or "0011111",
 * however we now that it must be followed by one differenitally encoded
 * bit, so we can use also the first symbol of data to force a more robust
 * detection, and look for one of the following:
 *
 * [001111]00
 * [0011111]00
 * [001111]01
 * [0011111]01
 */

#include "../proto6_core.h"
#include "tpms_build.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    if (numbits-6 < 64*2) return false; /* Ask for 64 bit of data (each bit
                                           is two symbols in the bitmap). */

    char *sync[] = {
        "00111100",
        "001111100",
        "00111101",
        "001111101",
        NULL
    };

    int j;
    uint32_t off = 0;
    for (j = 0; sync[j]; j++) {
        off = bitmap_seek_bits(bits,numbytes,0,numbits,sync[j]);
        if (off != BITMAP_SEEK_NOT_FOUND) {
            info->start_off = off;
            off += strlen(sync[j])-2;
            break;
	}
    }
    if (off == BITMAP_SEEK_NOT_FOUND) return false;

    FURI_LOG_E(TAG, "Toyota TPMS sync[%s] found", sync[j]);

    uint8_t raw[9];
    uint32_t decoded =
        convert_from_diff_manchester(raw,sizeof(raw),bits,numbytes,off,true);
    FURI_LOG_E(TAG, "Toyota TPMS decoded bits: %lu", decoded);

    if (decoded < 8*9) return false; /* Require the full 8 bytes. */
    if (crc8(raw,8,0x80,7) != raw[8]) return false; /* Require sane CRC. */

    /* We detected a valid signal. However now info->start_off is actually
     * pointing to the sync part, not the preamble of alternating 0 and 1.
     * Protoview decoders get called with some space to the left, in order
     * for the decoder itself to fix the signal if neeeded, so that its
     * logical representation will be more accurate and better to save
     * and retransmit. */
    if (info->start_off >= 12) {
        info->start_off -= 12;
        bitmap_set_pattern(bits,numbytes,info->start_off,"010101010101");
    }

    info->pulses_count = (off+8*9*2) - info->start_off;

    float psi = (float)((raw[4]&0x7f)<<1 | raw[5]>>7) * 0.25f - 7.0f;
    int temp = ((raw[5]&0x7f)<<1 | raw[6]>>7) - 40;

    fieldset_add_bytes(info->fieldset,"Tire ID",raw,4*2);
    fieldset_add_float(info->fieldset,"Pressure psi",psi,2);
    fieldset_add_int(info->fieldset,"Temperature C",temp,8);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 9U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 72U, 8U);
    return true;
}


static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[4] = {0x12, 0x34, 0x56, 0x78};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 8);
    fieldset_add_float(fieldset, "Pressure psi", 32.0f, 2);
    fieldset_add_int(fieldset, "Temperature C", 20, 8);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 48U;
    uint8_t data[9] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);

    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure psi");
    ProtoViewField* temperature = proto6_field_find(fieldset, "Temperature C");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        memcpy(data, id->bytes, 4U);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        int32_t pressure_code = (int32_t)((pressure->fvalue + 7.0f) * 4.0f + 0.5f);
        pressure_code = tpms_clamp_i32(pressure_code, 0, 255);
        data[4] = (uint8_t)((data[4] & 0x80U) | ((pressure_code >> 1U) & 0x7FU));
        data[5] = (uint8_t)((data[5] & 0x7FU) | ((pressure_code & 1U) << 7U));
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        const int32_t temp_code =
            tpms_clamp_i32((int32_t)temperature->value + 40, 0, 255);
        data[5] = (uint8_t)((data[5] & 0x80U) | ((temp_code >> 1U) & 0x7FU));
        data[6] = (uint8_t)((data[6] & 0x7FU) | ((temp_code & 1U) << 7U));
    }
    data[8] = crc8(data, 8U, 0x80U, 0x07U);

    tpms_add_pattern(samples, "010101010101001111", te);
    tpms_add_diff_manchester(samples, data, sizeof(data), te, true);
}

ProtoViewDecoder ToyotaTPMSDecoder = {
    .name = "Toyota TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};
