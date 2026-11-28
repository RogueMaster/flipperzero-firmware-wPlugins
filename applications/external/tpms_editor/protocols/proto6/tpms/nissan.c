/*  Nissan FSK 37 bit Manchester encoded checksummed TPMS data.

    Based on: https://github.com/merbanan/rtl_433/blob/master/src/devices/tpms_nissan.c
    and: https://www.reddit.com/r/RTLSDR/comments/wnvyzv/nissaninfiniti_tpms_sensor_decode_question/
    It is said to be e.g. PMV-C811 sensor, used in Nissan Leaf and Infniti
    (some Nissans are using the Renault protocol)

    Copyright (C) 2021 Alex Wilson <alex.david.wilson@gmail.com>
    Reference issue: https://github.com/merbanan/rtl_433/issues/1024

    Adapted to TPMS-Editor by TxF

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "../proto6_core.h"
#include "tpms_build.h"

#define NISSAN_DATA_BITS	37

// sum2N checksum, expects a 0x03 result for a valid message
// See https://github.com/merbanan/rtl_433/issues/1024#issuecomment-2592665745
static uint8_t tpms_nissan_checksum(uint8_t const *b)
{
    uint8_t chk = 0;
    for (int i = 0; i < 4; i++) {
        chk += b[i] >> 7;
        chk += b[i] >> 5;
        chk += b[i] >> 3;
        chk += b[i] >> 1;
        chk += (uint8_t)(b[i] << 1);
    }
    chk += b[4] >> 7;
    chk += b[4] >> 5;
    chk += b[4] >> 3;
    return ~chk & 0x03;
}

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    // it seems that the 36-bit preamble is "111101010101010101010101010101011110", followed by 37 bits od data GE Manchester coded
    // using just 28 bits of the preamble to match, in case we missed some part
    const char *sync_pattern = "0101010101010101010101011110";
    uint8_t sync_len = 28;
    if (numbits-sync_len < NISSAN_DATA_BITS) return false;		// done like for Ford, but shouldn't it account for Manchester as well?

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Nissan TPMS preamble found");

    info->start_off = off;
    off += sync_len; /* Skip preamble/sync. */

    // Data format: MODE:3d TPMS_ID:24h (PSI+THREE)*FOUR=8d UNKNOWN:2b
    // assuming that the "unknown" is just padding for the checksum to be correct - not saving this separatedly

    // example RawPayload: AF E5 EB B0 AA (mode: 101, ID: 0x7F2F5D, pressure 0x85, padding 01, three bits trailing not part of the data)

    // this gives the bit length info
    info->pulses_count = (off+NISSAN_DATA_BITS*2) - info->start_off;

    uint8_t raw[5]; raw[4] = 0; // zeroize last byte as it might not be fully filled during the decode
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10");
    FURI_LOG_E(TAG, "Nissan TPMS decoded bits: %lu", decoded);

    if (decoded < NISSAN_DATA_BITS) return false; /* Require 37 bits. */

    if (tpms_nissan_checksum(raw) != 0) return false; /* require sane CRC */
    FURI_LOG_E(TAG, "Nissan checksum ok");

    uint8_t mode = raw[0] >> 5;
    uint32_t id = (unsigned)((raw[0] & 0x1F) << 19) | (raw[1] << 11) | (raw[2] << 3) | (raw[3] >> 5);
    uint8_t id24bit[3];
    id24bit[0] = (id >> 16) & 0xFF;
    id24bit[1] = (id >> 8) & 0xFF;
    id24bit[2] = id & 0xFF;

    uint8_t pressure_raw  = ((raw[3] & 0x1F) << 3) | (raw[4] >> 5);
    float pressure_psi = (float)(pressure_raw / 4.0f - 3.0f);

    fieldset_add_bytes(info->fieldset,"Tire ID",id24bit,3*2);
    fieldset_add_float(info->fieldset,"Pressure psi",pressure_psi,2);
    fieldset_add_hex(info->fieldset,"Flags",mode,3);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 5U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 40U, 8U);
    return true;
}

/* Give fields and defaults for the signal creator. */
static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[3] = {0x12, 0x34, 0x56};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 6);
    fieldset_add_float(fieldset, "Pressure psi", 32.0f, 2);
    fieldset_add_hex(fieldset,"Flags",0x5,3);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {

    FURI_LOG_E(TAG, "Building Nissan frame");

    const uint32_t te = 120U;	// single pulse time
    tpms_add_pattern(samples, "111101010101010101010101010101011110", te);	// preamble and sync

/* Nissan sample preparation based on data */

    uint8_t data[5] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure psi");
    ProtoViewField* mode = proto6_field_find(fieldset, "Flags");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_FLAGS) && mode && mode->uvalue) {
        uint8_t mode_raw = mode->uvalue;

        data[0] &= 0x1FU;			// clear three highest bits
        data[0] |= (uint8_t)(mode_raw << 5);	// set mode
    }

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        data[0] &= 0xE0U;			// clear five lowest bits
        data[1] = 0;
        data[2] = 0;
        data[3] &= 0x1FU;			// clear three highest

        data[0] |= (uint8_t)(id->bytes[0] >> 3);  // ID starts here
        data[1] |= (uint8_t)(id->bytes[0] << 5);
        data[1] |= (uint8_t)(id->bytes[1] >> 3);
        data[2] |= (uint8_t)(id->bytes[1] << 5);
        data[2] |= (uint8_t)(id->bytes[2] >> 3);
        data[3] |= (uint8_t)(id->bytes[2] << 5);
    }

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE) && pressure && pressure->fvalue) {
        uint8_t pressure_raw = (uint8_t)((pressure->fvalue + 3.0f)*4.0f);

        data[3] &= 0xE0U;			// clear five lowest
        data[4] = 0;

        data[3] |= (uint8_t)(pressure_raw >> 3);
        data[4] |= (uint8_t)(pressure_raw << 5);
    }

    uint8_t fix = tpms_nissan_checksum(data);		// 0x0 if checksum is ok, 0x01, 0x02 or 0x03 if not
    if (fix != 0) data[4] |= (uint8_t)(fix << 3);	// use the padding area to fix the checksum if needed

    // write only required bits
    tpms_add_manchester_bits(samples, data, sizeof(data), NISSAN_DATA_BITS, te);
}

ProtoViewDecoder NissanTPMSDecoder = {
    .name = "Nissan PMV-C811 TEST",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};

