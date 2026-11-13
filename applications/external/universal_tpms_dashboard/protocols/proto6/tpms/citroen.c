/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Citroen TPMS. Usually 443.92 Mhz FSK.
 *
 * Preamble of ~14 high/low 52 us pulses
 * Sync of high 100us pulse then 50us low
 * Then Manchester bits, 10 bytes total.
 * Simple XOR checksum. */

#include "../proto6_core.h"
#include "tpms_build.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    /* We consider a preamble of 17 symbols. They are more, but the decoding
     * is more likely to happen if we don't pretend to receive from the
     * very start of the message. */
    uint32_t sync_len = 17;
    const char *sync_pattern = "10101010101010110";
    if (numbits-sync_len < 8*10) return false; /* Expect 10 bytes. */

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Renault TPMS preamble+sync found");

    info->start_off = off;
    off += sync_len; /* Skip preamble + sync. */

    uint8_t raw[10];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester. */
    FURI_LOG_E(TAG, "Citroen TPMS decoded bits: %lu", decoded);

    if (decoded < 8*10) return false; /* Require the full 10 bytes. */

    /* Match current rtl_433 sanity filtering. Zero pressure/temperature code
       is a common false decode and produced the observed -50 C capture. */
    if(raw[6] == 0U || raw[7] == 0U) return false;

    /* Check the CRC. It's a simple XOR of bytes 1-9, the first byte
     * is not included. The meaning of the first byte is unknown and
     * we don't display it. */
    uint8_t crc = 0;
    for (int j = 1; j < 10; j++) crc ^= raw[j];
    if (crc != 0) return false; /* Require sane checksum. */

    info->pulses_count = (off+8*10*2) - info->start_off;

    int repeat = raw[5] & 0xf;
    /* Tester captures with state 0xDC (Continental/VDO/FCA family) are
       consistently exactly x2 versus the classic PSA 1.364 kPa/count scale.
       Keep classic frames unchanged and apply the observed x2 variant only
       when the state byte identifies that family. */
    const float pressure_step = raw[0] == 0xDCU ? 2.728f : 1.364f;
    float kpa = (float)raw[6] * pressure_step;
    int temp = raw[7]-50;
    int battery = raw[8]; /* This may be the battery. It's not clear. */

    fieldset_add_bytes(info->fieldset,"Tire ID",raw+1,4*2);
    fieldset_add_float(info->fieldset,"Pressure kpa",kpa,2);
    fieldset_add_int(info->fieldset,"Temperature C",temp,8);
    fieldset_add_uint(info->fieldset,"Repeat",repeat,4);
    fieldset_add_uint(info->fieldset,"Battery",battery,8);
    fieldset_add_hex(info->fieldset,"State",raw[0],8);
    fieldset_add_uint(info->fieldset,"Pressure scale",raw[0] == 0xDCU ? 2U : 1U,2);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 10U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 80U, 8U);
    return true;
}


static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[4] = {0x12, 0x34, 0x56, 0x78};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 8);
    fieldset_add_float(fieldset, "Pressure kpa", 220.0f, 2);
    fieldset_add_int(fieldset, "Temperature C", 20, 8);
    fieldset_add_uint(fieldset, "Repeat", 1, 4);
    fieldset_add_uint(fieldset, "Battery", 100, 8);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 52U;
    tpms_add_pattern(samples, "10101010101010110", te);

    uint8_t data[10] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fieldset, "Temperature C");
    ProtoViewField* repeat = proto6_field_find(fieldset, "Repeat");
    ProtoViewField* battery = proto6_field_find(fieldset, "Battery");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        memcpy(data + 1U, id->bytes, 4U);
    }
    if(!have_raw) {
        data[0] = 0U;
        data[5] = (uint8_t)(repeat->uvalue & 0x0FU);
        data[8] =
            (uint8_t)tpms_clamp_u32((uint32_t)battery->uvalue, 255U);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        const float pressure_step = have_raw && data[0] == 0xDCU ? 2.728f : 1.364f;
        data[6] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue / pressure_step + 0.5f), 0, 255);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[7] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    data[9] = 0U;
    for(size_t i = 1U; i < 9U; i++) data[9] ^= data[i];
    tpms_add_manchester(samples, data, sizeof(data), te);
}

ProtoViewDecoder CitroenTPMSDecoder = {
    .name = "Citroen TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};
