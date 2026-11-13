/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license.
 *
 * Ford tires TPMS. Usually 443.92 Mhz FSK (in Europe).
 *
 * 52 us short pules
 * Preamble: 0101010101010101010101010101
 * Sync: 0110 (that is 52 us gap + 104 us pulse + 52 us gap)
 * Data: 8 bytes Manchester encoded
 * 01 = zero
 * 10 = one
 */

#include "../proto6_core.h"
#include "tpms_build.h"

static bool decode(uint8_t *bits, uint32_t numbytes, uint32_t numbits, ProtoViewMsgInfo *info) {

    const char *sync_pattern = "010101010101" "0110";
    uint8_t sync_len = 12+4; /* We just use 12 preamble symbols + sync. */
    if (numbits-sync_len < 8*8) return false;

    uint64_t off = bitmap_seek_bits(bits,numbytes,0,numbits,sync_pattern);
    if (off == BITMAP_SEEK_NOT_FOUND) return false;
    FURI_LOG_E(TAG, "Fort TPMS preamble+sync found");

    info->start_off = off;
    off += sync_len; /* Skip preamble and sync. */

    uint8_t raw[8];
    uint32_t decoded =
        convert_from_line_code(raw,sizeof(raw),bits,numbytes,off,
            "01","10"); /* Manchester. */
    FURI_LOG_E(TAG, "Ford TPMS decoded bits: %lu", decoded);

    if (decoded < 8*8) return false; /* Require the full 8 bytes. */

    /* CRC is just the sum of the first 7 bytes MOD 256. */
    uint8_t crc = 0;
    for (int j = 0; j < 7; j++) crc += raw[j];
    if (crc != raw[7]) return false; /* Require sane CRC. */

    info->pulses_count = (off+8*8*2) - info->start_off;

    float psi = 0.25f * (((raw[6]&0x20)<<3)|raw[4]);

    /* TPMS 4.4: treat Ford temperature as valid only when both conditions
     * observed in current captures are satisfied:
     *   - TT MSB is clear, and
     *   - the low nibble of FF is not 0xB (xxxx1011).
     * For invalid/unknown TT states we preserve raw TT/FF but deliberately do
     * not publish a Temperature C field, so the UI can show "--" instead of
     * inventing a numeric temperature. */
    const bool temperature_valid =
        ((raw[5] & 0x80U) == 0U) && ((raw[6] & 0x0FU) != 0x0BU);
    const int temp = (int)raw[5] - 56;
    /* TT is normally temperature. If its MSB is set rtl_433 treats it as
       unknown status-like data. Preserve that low 7-bit TT value in the high
       byte, and always preserve the real Ford flags/status byte raw[6].
       Examples: normal 0x0014/0x006C; special TT=0xDC, FF=0x6C -> 0x5C6C. */
    const uint16_t status =
        (uint16_t)(((raw[5] & 0x80U) ? (raw[5] & 0x7FU) : 0U) << 8U) | raw[6];
    /* Real same-sensor captures show 0x0B -> 0x4B and 0x06 -> 0x46
       transitions, which isolates 0x40 as the motion bit while lower state
       bits remain independent. */
    int car_moving = (raw[6] & 0x40U) != 0U;

    fieldset_add_bytes(info->fieldset,"Tire ID",raw,4*2);
    fieldset_add_float(info->fieldset,"Pressure psi",psi,2);
    if(temperature_valid) fieldset_add_int(info->fieldset,"Temperature C",temp,8);
    fieldset_add_uint(info->fieldset,"Temperature valid",temperature_valid ? 1U : 0U,1);
    fieldset_add_hex(info->fieldset,"Status",status,16);
    fieldset_add_hex(info->fieldset,"TT raw",raw[5],8);
    fieldset_add_hex(info->fieldset,"FF raw",raw[6],8);
    fieldset_add_uint(info->fieldset,"Moving",car_moving,1);
    /* Keep the upstream/legacy 0x08 Learn interpretation, but expose 0x10 and
       0x80 separately as RAW diagnostics. Real captures show those bits and
       their exact semantics are not established well enough to rename them. */
    fieldset_add_uint(info->fieldset,"Learn",(raw[6] & 0x08U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Learn 0x10",(raw[6] & 0x10U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Pressure bit9",(raw[6] & 0x20U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Flag 0x80",(raw[6] & 0x80U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Flag 0x04",(raw[6] & 0x04U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Flag 0x02",(raw[6] & 0x02U) != 0U,1);
    fieldset_add_uint(info->fieldset,"Flag 0x01",(raw[6] & 0x01U) != 0U,1);
    fieldset_add_hex(info->fieldset,"State bits",raw[6] & 0x5FU,8);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 8U * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", 64U, 8U);
    return true;
}


static void get_fields(ProtoViewFieldSet* fieldset) {
    const uint8_t default_id[4] = {0x12, 0x34, 0x56, 0x78};
    fieldset_add_bytes(fieldset, "Tire ID", default_id, 8);
    fieldset_add_float(fieldset, "Pressure psi", 32.0f, 2);
    fieldset_add_int(fieldset, "Temperature C", 20, 8);
    fieldset_add_uint(fieldset, "Moving", 0, 1);
}

static void build_message(RawSamplesBuffer* samples, ProtoViewFieldSet* fieldset) {
    const uint32_t te = 52U;
    tpms_add_pattern(samples, "01010101010101010101010101010110", te);

    uint8_t data[8] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fieldset, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fieldset, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fieldset, "Pressure psi");
    ProtoViewField* temperature = proto6_field_find(fieldset, "Temperature C");
    ProtoViewField* moving = proto6_field_find(fieldset, "Moving");

    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_ID) && id && id->bytes) {
        memcpy(data, id->bytes, 4U);
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_PRESSURE)) {
        const int32_t pressure_code = tpms_clamp_i32(
            (int32_t)(pressure->fvalue * 4.0f + 0.5f), 0, 511);
        data[4] = (uint8_t)(pressure_code & 0xFFU);
        data[6] = (uint8_t)(
            (data[6] & ~0x20U) | (((pressure_code >> 8U) & 1U) << 5U));
    }
    if(tpms_build_should_patch(fieldset, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[5] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 56, 0, 127);
    }
    if(!have_raw) {
        data[6] &= (uint8_t)~0x44U;
        if(moving && moving->uvalue) data[6] |= 0x44U;
    }
    data[7] = 0U;
    for(size_t i = 0U; i < 7U; i++) {
        data[7] = (uint8_t)(data[7] + data[i]);
    }
    tpms_add_manchester(samples, data, sizeof(data), te);
}

ProtoViewDecoder FordTPMSDecoder = {
    .name = "Ford TPMS",
    .decode = decode,
    .get_fields = get_fields,
    .build_message = build_message
};
