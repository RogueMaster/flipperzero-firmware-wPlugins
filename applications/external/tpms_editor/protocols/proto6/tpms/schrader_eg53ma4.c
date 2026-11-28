/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Schrader variant EG53MA4 TPMS.
 *
 * TPMS 2.3 notes (EG53 fixes carried from 2.2):
 * - payload layout aligned with current rtl_433 EG53MA4 decoder
 * - pressure scale is 2.5 kPa/count (25 mbar/count)
 * - full 32-bit status/flags are preserved
 * - the original Fahrenheit byte is preserved for RX -> edit -> TX
 * - Manchester symbol width is 123 us
 *
 * Payload: 10 bytes, Manchester encoded:
 *   [0..3] flags/status
 *   [4..6] 24-bit sensor ID
 *   [7]    pressure, 2.5 kPa/count
 *   [8]    temperature in degrees Fahrenheit
 *   [9]    checksum = sum(data[0..8]) modulo 256
 */

#include "../proto6_core.h"
#include "tpms_build.h"

static bool decode(uint8_t* bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo* info) {
    const char* sync_pattern = "010101010101" "01100101";
    const uint8_t sync_len = 12U + 8U;
    if(numbits - sync_len + 8U < 8U * 10U) return false;

    uint64_t off = bitmap_seek_bits(bits, numbytes, 0, numbits, sync_pattern);
    if(off == BITMAP_SEEK_NOT_FOUND) return false;

    info->start_off = off;
    off += sync_len - 8U;

    uint8_t raw[10] = {0};
    const uint32_t decoded =
        convert_from_line_code(raw, sizeof(raw), bits, numbytes, off, "01", "10");
    if(decoded < 10U * 8U) return false;

    uint8_t checksum = 0U;
    for(size_t j = 0U; j < 9U; j++) checksum = (uint8_t)(checksum + raw[j]);
    if(checksum != raw[9]) return false;

    info->pulses_count = (off + 10U * 8U * 2U) - info->start_off;

    const uint32_t flags = ((uint32_t)raw[0] << 24U) | ((uint32_t)raw[1] << 16U) |
                           ((uint32_t)raw[2] << 8U) | (uint32_t)raw[3];
    const float kpa = (float)raw[7] * 2.5f;
    const int32_t temp_f = raw[8];
    const int32_t temp_c = (int32_t)(((float)temp_f - 32.0f) * (5.0f / 9.0f));

    fieldset_add_bytes(info->fieldset, "Tire ID", raw + 4, 3U * 2U);
    fieldset_add_float(info->fieldset, "Pressure kpa", kpa, 2U);
    fieldset_add_int(info->fieldset, "Temperature C", temp_c, 8U);
    fieldset_add_hex(info->fieldset, "Flags", flags, 32U);
    fieldset_add_hex(info->fieldset, "Temperature F Raw", raw[8], 8U);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 10U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 80U, 8U);
    return true;
}

static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[3] = {0x12, 0x34, 0x56};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 6U);
    fieldset_add_float(fieldset, "Pressure kpa", 220.0f, 2U);
    fieldset_add_int(fieldset, "Temperature C", 20, 8U);
    fieldset_add_hex(fieldset, "Flags", 0x4D030060U, 32U);
    fieldset_add_hex(fieldset, "Temperature F Raw", 68U, 8U);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 123U;
    uint8_t data[10] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);

    ProtoViewField* id_field = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure_field = proto6_field_find(fieldset, "Pressure kpa");
    ProtoViewField* temperature_field = proto6_field_find(fieldset, "Temperature C");
    ProtoViewField* flags_field = proto6_field_find(fieldset, "Flags");
    ProtoViewField* temp_raw_field =
        proto6_field_find(fieldset, "Temperature F Raw");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_FLAGS)) {
        const uint32_t flags =
            flags_field ? (uint32_t)flags_field->uvalue : 0x4D030060U;
        data[0] = (uint8_t)(flags >> 24U);
        data[1] = (uint8_t)(flags >> 16U);
        data[2] = (uint8_t)(flags >> 8U);
        data[3] = (uint8_t)flags;
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) &&
       id_field && id_field->bytes) {
        memcpy(data + 4U, id_field->bytes, 3U);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        const int32_t pressure_raw =
            (int32_t)(pressure_field->fvalue / 2.5f + 0.5f);
        data[7] = (uint8_t)tpms_clamp_i32(pressure_raw, 0, 255);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        if(temp_raw_field) {
            data[8] = (uint8_t)tpms_clamp_i32(
                (int32_t)temp_raw_field->uvalue, 0, 255);
        } else {
            const int32_t temp_f = (int32_t)(
                (float)temperature_field->value * 9.0f / 5.0f + 32.0f + 0.5f);
            data[8] = (uint8_t)tpms_clamp_i32(temp_f, 0, 255);
        }
    }
    data[9] = 0U;
    for(size_t i = 0U; i < 9U; i++) {
        data[9] = (uint8_t)(data[9] + data[i]);
    }

    tpms_add_pattern(samples, "010101010101", te);
    tpms_add_manchester(samples, data, sizeof(data), te);
}

ProtoViewDecoder SchraderEG53MA4TPMSDecoder = {
    .name = "Schrader EG53MA4 TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message,
};
