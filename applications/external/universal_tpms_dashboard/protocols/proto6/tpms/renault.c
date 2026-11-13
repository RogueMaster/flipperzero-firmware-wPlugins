/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Renault tires TPMS. Usually 443.92 Mhz FSK.
 *
 * Preamble + sync + Manchester bits. ~48us short pulse.
 * 9 Bytes in total not counting the preamble. */

#include "../proto6_core.h"
#include "tpms_build.h"

#define USE_TEST_VECTOR 0
static const char *test_vector =
    "...01010101010101010110" // Preamble + sync

    /* The following is Marshal encoded, so each two characters are
     * actaully one bit. 01 = 0, 10 = 1. */
    "010110010110" // Flags.
    "10011001101010011001" // Pressure, multiply by 0.75 to obtain kpa.
                           // 244 kpa here.
    "1010010110011010"  // Temperature, subtract 30 to obtain celsius. 22C here.
    "1001010101101001"
    "0101100110010101"
    "1001010101100110"  // Tire ID. 0x7AD779 here.
    "0101010101010101"
    "0101010101010101"  // Two FF bytes (usually). Unknown.
    "0110010101010101"; // CRC8 with (poly 7, initialization 0).

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    if (USE_TEST_VECTOR) { /* Test vector to check that decoding works. */
        bitmap_set_pattern(bits,numbytes,0,test_vector);
        numbits = strlen(test_vector);
    }

    if (numbits-12 < 9*8) return false;

    const char *sync_pattern = "01010101010101010110";
    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Renault TPMS preamble+sync found");

    info->start_off = off;
    off += 20; /* Skip preamble. */

    uint8_t raw[9];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester. */
    FURI_LOG_E(TAG, "Renault TPMS decoded bits: %lu", decoded);

    if (decoded < 8*9) return false; /* Require the full 9 bytes. */
    if (crc8(raw,8,0,7) != raw[8]) return false; /* Require sane CRC. */

    info->pulses_count = (off+8*9*2) - info->start_off;

    uint8_t flags = raw[0]>>2;
    float kpa = 0.75f * ((uint32_t)((raw[0]&3)<<8) | raw[1]);
    int temp = raw[2]-30;

    /* FIX2 regression: the tester supplied three CRC-valid false positives
       beginning DC 8D F5/F6. Interpreting them as Renault produced impossible
       temperatures of 215-216 C. Keep the same range as the editor and reject
       such cross-protocol/random matches before publishing or Autosaving. */
    if(temp < -50 || temp > 150) return false;

    fieldset_add_bytes(info->fieldset,"Tire ID",raw+3,3*2);
    fieldset_add_float(info->fieldset,"Pressure kpa",kpa,2);
    fieldset_add_int(info->fieldset,"Temperature C",temp,8);
    fieldset_add_hex(info->fieldset,"Flags",flags,6);
    fieldset_add_bytes(info->fieldset,"Unknown1",raw+6,2);
    fieldset_add_bytes(info->fieldset,"Unknown2",raw+7,2);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 9U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 72U, 8U);
    return true;
}

/* Give fields and defaults for the signal creator. */
static void get_fields(ProtoViewFieldSet *fieldset) {
    uint8_t default_id[3]= {0xAB, 0xCD, 0xEF};
    fieldset_add_bytes(fieldset,"Tire ID",default_id,3*2);
    fieldset_add_float(fieldset,"Pressure kpa",123,2);
    fieldset_add_int(fieldset,"Temperature C",20,8);
    // We don't know what flags are, but 1B is a common value.
    fieldset_add_hex(fieldset,"Flags",0x1b,6);
    fieldset_add_bytes(fieldset,"Unknown1",(uint8_t*)"\xff",2);
    fieldset_add_bytes(fieldset,"Unknown2",(uint8_t*)"\xff",2);
}

/* Create a Renault TPMS signal, according to the fields provided. */
static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 50U;
    tpms_add_pattern(samples, "01010101010101010101010101010110", te);

    uint8_t data[9] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);

    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fieldset, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fieldset, "Flags");
    ProtoViewField* unknown1 = proto6_field_find(fieldset, "Unknown1");
    ProtoViewField* unknown2 = proto6_field_find(fieldset, "Unknown2");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        memcpy(data + 3U, id->bytes, 3U);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        const uint32_t raw_pressure = (uint32_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue / 0.75f + 0.5f), 0, 1023);
        data[0] = (uint8_t)((data[0] & 0xFCU) | ((raw_pressure >> 8U) & 3U));
        data[1] = (uint8_t)raw_pressure;
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[2] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 30, 0, 255);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_FLAGS)) {
        data[0] = (uint8_t)((data[0] & 0x03U) | ((flags->uvalue & 0x3FU) << 2U));
    }

    if(!have_raw) {
        if(unknown1 && unknown1->bytes) data[6] = unknown1->bytes[0];
        if(unknown2 && unknown2->bytes) data[7] = unknown2->bytes[0];
    }

    /* Recalculate only the derived integrity byte; for an unedited valid RX
     * this produces the same byte while keeping all unknown bytes untouched. */
    data[8] = crc8(data, 8U, 0U, 7U);
    tpms_add_manchester(samples, data, sizeof(data), te);
}

ProtoViewDecoder RenaultTPMSDecoder = {
    .name = "Renault TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};
