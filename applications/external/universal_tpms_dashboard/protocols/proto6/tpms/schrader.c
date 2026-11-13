/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Schrader TPMS. Usually 443.92 Mhz OOK, 120us pulse len.
 *
 * 500us high pulse + Preamble + Manchester coded bits where:
 * 1 = 10
 * 0 = 01
 *
 * 60 bits of data total (first 4 nibbles is the preamble, 0xF).
 *
 * Used in FIAT-Chrysler, Mercedes, ... */

#include "../proto6_core.h"
#include "tpms_build.h"

#define USE_TEST_VECTOR 0
static const char *test_vector = "000000111101010101011010010110010110101001010110100110011001100101010101011010100110100110011010101010101010101010101010101010101010101010101010";

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    if (USE_TEST_VECTOR) { /* Test vector to check that decoding works. */
        bitmap_set_pattern(bits,numbytes,0,test_vector);
        numbits = strlen(test_vector);
    }

    if (numbits < 64) return false; /* Preamble + data. */

    const char *sync_pattern = "1111010101" "01011010";
    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Schrader TPMS gap+preamble found");

    info->start_off = off;
    off += 10; /* Skip just the long pulse and the first 3 bits of sync, so
                  that we have the first byte of data with the sync nibble
                  0011 = 0x3. */

    uint8_t raw[8];
    uint8_t id[4];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester code. */
    FURI_LOG_E(TAG, "Schrader TPMS decoded bits: %lu", decoded);

    if (decoded < 64) return false; /* Require the full 8 bytes. */

    raw[0] |= 0xf0; // Fix the preamble nibble for checksum computation.
    uint8_t cksum = crc8(raw,sizeof(raw)-1,0xf0,0x7);
    if (cksum != raw[7]) {
        FURI_LOG_E(TAG, "Schrader TPMS checksum mismatch");
        return false;
    }

    info->pulses_count = (off+8*8*2) - info->start_off;

    float kpa = (float)raw[5]*2.5f;
    int temp = raw[6]-50;
    id[0] = raw[1]&7;
    id[1] = raw[2];
    id[2] = raw[3];
    id[3] = raw[4];

    fieldset_add_bytes(info->fieldset,"Tire ID",id,4*2);
    fieldset_add_float(info->fieldset,"Pressure kpa",kpa,2);
    fieldset_add_int(info->fieldset,"Temperature C",temp,8);
    fieldset_add_hex(info->fieldset,"Status",raw[0] & 0x0F,4);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 8U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 64U, 8U);
    return true;
}


static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[4] = {0x01, 0x23, 0x45, 0x67};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 8);
    fieldset_add_float(fieldset, "Pressure kpa", 220.0f, 2);
    fieldset_add_int(fieldset, "Temperature C", 20, 8);
    fieldset_add_hex(fieldset, "Status", 0, 4);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 120U;
    uint8_t crc_data[8] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, crc_data, sizeof(crc_data), NULL) ==
        sizeof(crc_data);

    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fieldset, "Temperature C");
    ProtoViewField* status = proto6_field_find(fieldset, "Status");

    /* RX stores the checksum representation with high preamble nibble repaired
     * to F. Keep that representation internally and restore 0x3 on air. */
    crc_data[0] |= 0xF0U;
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_FLAGS)) {
        crc_data[0] =
            (uint8_t)(0xF0U | (status ? (status->uvalue & 0x0FU) : 0U));
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        crc_data[1] =
            (uint8_t)((crc_data[1] & 0xF8U) | (id->bytes[0] & 0x07U));
        crc_data[2] = id->bytes[1];
        crc_data[3] = id->bytes[2];
        crc_data[4] = id->bytes[3];
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        const int32_t pressure_raw =
            (int32_t)(pressure->fvalue / 2.5f + 0.5f);
        crc_data[5] =
            (uint8_t)tpms_clamp_i32(pressure_raw, 0, 255);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        crc_data[6] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    crc_data[7] = crc8(crc_data, 7U, 0xF0U, 0x07U);

    uint8_t tx_data[8];
    memcpy(tx_data, crc_data, sizeof(tx_data));
    tx_data[0] = (uint8_t)(0x30U | (crc_data[0] & 0x0FU));
    tpms_add_pattern(samples, "0000001111010101", te);
    tpms_add_manchester(samples, tx_data, sizeof(tx_data), te);
}

ProtoViewDecoder SchraderTPMSDecoder = {
    .name = "Schrader TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};
