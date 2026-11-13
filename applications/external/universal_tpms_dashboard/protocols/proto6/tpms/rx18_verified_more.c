/*
 * TPMS 2.6 TEST: four additional RX/EDIT/TX codecs whose payload layouts and
 * integrity checks are documented by the rtl_433 source core used by the
 * companion SDR# plugin.
 *
 * Added test codecs:
 *   - Toyota/Pacific PMV-107J (315 MHz, FSK, differential Manchester)
 *   - VDO TG1C / Abarth 124 (433.92 MHz, FSK, Manchester)
 *   - Shenzhen EGQ Q85 (433.92 MHz, FSK, Manchester)
 *   - Mercedes-Benz Sprinter 4500 (FSK, Manchester, 25 us)
 *
 * The builders are lossless-aware: when an RX raw payload is present they
 * start from it, overwrite only fields selected by Edit mask, then recompute
 * the protocol integrity field(s).
 */

#include "../proto6_core.h"
#include "tpms_build.h"

#define RX18_NOT_FOUND UINT32_MAX

static bool rx18_get_bit(
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t pos,
    bool invert) {
    bool value = bitmap_get((uint8_t*)bits, numbytes, pos);
    return invert ? !value : value;
}

static bool rx18_pattern_bit(const uint8_t* pattern, uint32_t pos) {
    return ((pattern[pos / 8U] >> (7U - (pos & 7U))) & 1U) != 0U;
}

static uint32_t rx18_seek_pattern(
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    uint32_t start,
    const uint8_t* pattern,
    uint32_t pattern_bits,
    bool invert) {
    if(!bits || !pattern || pattern_bits == 0U || start >= numbits) return RX18_NOT_FOUND;
    for(uint32_t pos = start; pos + pattern_bits <= numbits; pos++) {
        bool match = true;
        for(uint32_t i = 0U; i < pattern_bits; i++) {
            if(rx18_get_bit(bits, numbytes, pos + i, invert) != rx18_pattern_bit(pattern, i)) {
                match = false;
                break;
            }
        }
        if(match) return pos;
    }
    return RX18_NOT_FOUND;
}

/* rtl_433 Manchester convention used by the source codecs: 01 -> 1, 10 -> 0. */
static uint32_t rx18_manchester_decode(
    uint8_t* out,
    uint32_t outbytes,
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    uint32_t start,
    uint32_t max_decoded,
    bool invert,
    uint32_t* consumed_raw) {
    if(!out || !outbytes || !bits) return 0U;
    memset(out, 0, outbytes);
    uint32_t decoded = 0U;
    uint32_t pos = start;
    const uint32_t capacity = outbytes * 8U;
    while(pos + 1U < numbits && decoded < max_decoded && decoded < capacity) {
        const bool a = rx18_get_bit(bits, numbytes, pos, invert);
        const bool b = rx18_get_bit(bits, numbytes, pos + 1U, invert);
        if(a == b) break;
        bitmap_set(out, outbytes, decoded++, (!a && b));
        pos += 2U;
    }
    if(consumed_raw) *consumed_raw = pos - start;
    return decoded;
}

/* Clock alignment equivalent to rtl_433 differential Manchester decoding. */
static uint32_t rx18_diff_manchester_decode(
    uint8_t* out,
    uint32_t outbytes,
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    uint32_t start,
    uint32_t max_decoded,
    bool invert,
    uint32_t* consumed_raw) {
    if(!out || !outbytes || !bits || start >= numbits) return 0U;
    memset(out, 0, outbytes);
    uint32_t end = numbits;
    if(max_decoded && end > start + max_decoded * 2U) end = start + max_decoded * 2U;
    uint32_t pos = start;
    uint32_t decoded = 0U;
    bool bit1 = false;
    bool bit2 = false;

    while(pos + 2U < end) {
        bit1 = rx18_get_bit(bits, numbytes, pos++, invert);
        bit2 = rx18_get_bit(bits, numbytes, pos++, invert);
        const bool bit3 = rx18_get_bit(bits, numbytes, pos, invert);
        if(bit1 != bit2) {
            if(bit2 != bit3) {
                bitmap_set(out, outbytes, decoded++, false);
                break;
            } else {
                bit2 = bit1;
                pos -= 1U;
                break;
            }
        } else {
            bit2 = !bit1;
            pos -= 2U;
            break;
        }
    }

    while(pos + 1U < end && decoded < max_decoded && decoded < outbytes * 8U) {
        bit1 = rx18_get_bit(bits, numbytes, pos++, invert);
        if(bit1 == bit2) break;
        bit2 = rx18_get_bit(bits, numbytes, pos++, invert);
        bitmap_set(out, outbytes, decoded++, bit1 == bit2);
    }
    if(consumed_raw) *consumed_raw = pos - start;
    return decoded;
}

static void rx18_extract_bits(
    uint8_t* dst,
    uint32_t dstbytes,
    const uint8_t* src,
    uint32_t srcbytes,
    uint32_t start,
    uint32_t count) {
    memset(dst, 0, dstbytes);
    if(count > dstbytes * 8U) count = dstbytes * 8U;
    for(uint32_t i = 0U; i < count; i++) {
        bitmap_set(dst, dstbytes, i, bitmap_get((uint8_t*)src, srcbytes, start + i));
    }
}

static void rx18_insert_bits(
    uint8_t* dst,
    uint32_t dstbytes,
    uint32_t start,
    const uint8_t* src,
    uint32_t srcbytes,
    uint32_t count) {
    if(start >= dstbytes * 8U) return;
    if(count > srcbytes * 8U) count = srcbytes * 8U;
    if(start + count > dstbytes * 8U) count = dstbytes * 8U - start;
    for(uint32_t i = 0U; i < count; i++) {
        bitmap_set(dst, dstbytes, start + i, bitmap_get((uint8_t*)src, srcbytes, i));
    }
}

static void rx18_add_manchester_bit(
    RawSamplesBuffer* samples,
    bool value,
    uint32_t te,
    bool invert) {
    bool first = value ? false : true;
    bool second = !first;
    if(invert) {
        first = !first;
        second = !second;
    }
    raw_samples_add_or_update(samples, first, te);
    raw_samples_add_or_update(samples, second, te);
}

static void rx18_add_manchester_bits(
    RawSamplesBuffer* samples,
    const uint8_t* data,
    uint32_t bits,
    uint32_t te,
    bool invert) {
    const uint32_t bytes = (bits + 7U) / 8U;
    for(uint32_t bit = 0U; bit < bits; bit++) {
        rx18_add_manchester_bit(samples, bitmap_get((uint8_t*)data, bytes, bit), te, invert);
    }
}

static void rx18_add_diff_manchester_bits(
    RawSamplesBuffer* samples,
    const uint8_t* data,
    uint32_t bits,
    uint32_t te,
    bool previous) {
    const uint32_t bytes = (bits + 7U) / 8U;
    for(uint32_t bit = 0U; bit < bits; bit++) {
        const bool value = bitmap_get((uint8_t*)data, bytes, bit);
        const bool first = !previous;
        const bool second = value ? first : previous;
        raw_samples_add_or_update(samples, first, te);
        raw_samples_add_or_update(samples, second, te);
        previous = second;
    }
}

static uint16_t rx18_crc16_ccitt_false(const uint8_t* message, uint32_t count) {
    uint16_t remainder = 0xFFFFU;
    for(uint32_t byte = 0U; byte < count; byte++) {
        remainder ^= (uint16_t)message[byte] << 8U;
        for(uint32_t bit = 0U; bit < 8U; bit++) {
            remainder = (remainder & 0x8000U) ?
                            (uint16_t)((remainder << 1U) ^ 0x1021U) :
                            (uint16_t)(remainder << 1U);
        }
    }
    return remainder;
}

static uint8_t rx18_xor8(const uint8_t* data, uint32_t count) {
    uint8_t value = 0U;
    for(uint32_t i = 0U; i < count; i++) value ^= data[i];
    return value;
}

static void rx18_add_id32(ProtoViewFieldSet* fields, uint32_t id) {
    const uint8_t bytes[4] = {
        (uint8_t)(id >> 24U),
        (uint8_t)(id >> 16U),
        (uint8_t)(id >> 8U),
        (uint8_t)id,
    };
    fieldset_add_bytes(fields, "Tire ID", bytes, 8U);
}

static uint32_t rx18_id32_from_field(const ProtoViewField* field) {
    if(!field || field->type != FieldTypeBytes || !field->bytes) return 0U;
    uint32_t count = (field->len + 1U) / 2U;
    if(count > 4U) count = 4U;
    uint32_t value = 0U;
    for(uint32_t i = 0U; i < count; i++) value = (value << 8U) | field->bytes[i];
    return value;
}

/* -------------------------------------------------------------------------- */
/* Toyota / Pacific PMV-107J                                                  */
/* -------------------------------------------------------------------------- */

static bool rx18_pmv107j_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble[] = {0xF8U}; /* 6 bits: 111110 */
    uint32_t pos = 0U;
    while((pos = rx18_seek_pattern(bits, numbytes, numbits, pos, preamble, 6U, false)) !=
          RX18_NOT_FOUND) {
        uint8_t packet[9] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx18_diff_manchester_decode(
            packet, sizeof(packet), bits, numbytes, numbits, pos + 6U, 67U, false, &consumed);
        if(decoded >= 66U) {
            /* rtl_433 realignment: six leading filler bits followed by the
             * 66 payload bits. */
            uint8_t raw[9] = {0};
            raw[0] = (uint8_t)(packet[0] >> 6U);
            for(uint32_t i = 0U; i < 64U; i++) {
                bitmap_set(raw, sizeof(raw), 8U + i, bitmap_get(packet, sizeof(packet), 2U + i));
            }

            if(crc8(raw, 8U, 0x00U, 0x13U) == raw[8] &&
               raw[5] == (uint8_t)(raw[6] ^ 0xFFU)) {
                const uint32_t id = ((uint32_t)raw[0] << 26U) | ((uint32_t)raw[1] << 18U) |
                                    ((uint32_t)raw[2] << 10U) | ((uint32_t)raw[3] << 2U) |
                                    (raw[4] >> 6U);
                const uint32_t status = raw[4] & 0x3FU;
                const float pressure_kpa = ((float)raw[5] - 40.0f) * 2.48f;
                const int32_t temperature_c = (int32_t)raw[7] - 40;
                if(pressure_kpa >= -100.0f && pressure_kpa <= 800.0f &&
                   temperature_c >= -40 && temperature_c <= 215) {
                    info->start_off = pos;
                    info->pulses_count = 6U + consumed;
                    rx18_add_id32(info->fieldset, id);
                    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 2U);
                    fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                    fieldset_add_hex(info->fieldset, "Status", status, 6U);
                    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 18U);
                    fieldset_add_uint(info->fieldset, "Raw payload bits", 72U, 8U);
                    return true;
                }
            }
        }
        pos += 2U;
    }
    return false;
}

static void rx18_pmv107j_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t default_id[4] = {0x01U, 0x23U, 0x45U, 0x67U};
    fieldset_add_bytes(fields, "Tire ID", default_id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 2U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Status", 0U, 6U);
}

static void rx18_pmv107j_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 100U;
    uint8_t raw[9] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, raw, sizeof(raw), NULL) == sizeof(raw);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* status = proto6_field_find(fields, "Status");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        const uint32_t value = rx18_id32_from_field(id) & 0x0FFFFFFFU;
        raw[0] = (uint8_t)(value >> 26U);
        raw[1] = (uint8_t)(value >> 18U);
        raw[2] = (uint8_t)(value >> 10U);
        raw[3] = (uint8_t)(value >> 2U);
        raw[4] = (uint8_t)((raw[4] & 0x3FU) | ((value & 0x03U) << 6U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        raw[4] = (uint8_t)((raw[4] & 0xC0U) | (status ? (status->uvalue & 0x3FU) : 0U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        int32_t code = (int32_t)(pressure->fvalue / 2.48f + 40.0f + 0.5f);
        code = tpms_clamp_i32(code, 0, 255);
        raw[5] = (uint8_t)code;
        raw[6] = (uint8_t)(raw[5] ^ 0xFFU);
    } else {
        /* The inverse-pressure byte is derived, never independently editable. */
        raw[6] = (uint8_t)(raw[5] ^ 0xFFU);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        raw[7] = (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 40, 0, 255);
    }
    raw[8] = crc8(raw, 8U, 0x00U, 0x13U);

    uint8_t payload66[9] = {0};
    /* raw bits 6..71 are the 66 over-the-air data bits. */
    for(uint32_t i = 0U; i < 66U; i++) {
        bitmap_set(payload66, sizeof(payload66), i, bitmap_get(raw, sizeof(raw), 6U + i));
    }
    tpms_add_pattern(samples, "111110", te);
    rx18_add_diff_manchester_bits(samples, payload66, 66U, te, false);
}

ProtoViewDecoder PMV107JRX18TPMSDecoder = {
    .name = "Toyota PMV-107J TEST",
    .decode = rx18_pmv107j_decode,
    .get_fields = rx18_pmv107j_get_fields,
    .build_message = rx18_pmv107j_build,
};

/* -------------------------------------------------------------------------- */
/* VDO TG1C / Abarth 124 and Shenzhen Q85                                    */
/* -------------------------------------------------------------------------- */

static bool rx18_abarth_decode_common(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info,
    bool q85) {
    static const uint8_t preamble_inverted[] = {0xAAU, 0xAAU, 0xA9U};
    const uint32_t wanted_bits = q85 ? 96U : 72U;
    const uint32_t wanted_bytes = q85 ? 12U : 9U;
    uint32_t pos = 0U;
    while((pos = rx18_seek_pattern(
               bits, numbytes, numbits, pos, preamble_inverted, 24U, true)) != RX18_NOT_FOUND) {
        uint8_t raw[12] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx18_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 24U, wanted_bits, true, &consumed);
        if(decoded >= wanted_bits && rx18_xor8(raw, 9U) == 0U) {
            bool integrity_ok = true;
            if(q85) {
                const uint16_t expected = (uint16_t)raw[10] | ((uint16_t)raw[11] << 8U);
                integrity_ok = raw[9] == 0x40U && rx18_crc16_ccitt_false(raw, 10U) == expected;
            } else {
                /* Do not mislabel a valid Q85 frame as TG1C. */
                uint8_t qraw[12] = {0};
                uint32_t qconsumed = 0U;
                const uint32_t qdecoded = rx18_manchester_decode(
                    qraw, sizeof(qraw), bits, numbytes, numbits, pos + 24U, 96U, true, &qconsumed);
                if(qdecoded >= 96U && rx18_xor8(qraw, 9U) == 0U && qraw[9] == 0x40U) {
                    const uint16_t qexpected = (uint16_t)qraw[10] | ((uint16_t)qraw[11] << 8U);
                    if(rx18_crc16_ccitt_false(qraw, 10U) == qexpected) integrity_ok = false;
                }
            }
            if(integrity_ok) {
                const uint32_t id = ((uint32_t)raw[0] << 24U) | ((uint32_t)raw[1] << 16U) |
                                    ((uint32_t)raw[2] << 8U) | raw[3];
                const float pressure_kpa = (float)raw[5] * (q85 ? 3.0f : 1.38f);
                const int32_t temperature_c = (int32_t)raw[6] - (q85 ? 55 : 50);
                const int32_t min_temp = q85 ? -20 : -50;
                const int32_t max_temp = q85 ? 80 : 125;
                if(temperature_c >= min_temp && temperature_c <= max_temp && pressure_kpa <= 900.0f) {
                    info->start_off = pos;
                    info->pulses_count = 24U + consumed;
                    rx18_add_id32(info->fieldset, id);
                    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 2U);
                    fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                    /* Pack byte4 and status byte7 into the app's 32-bit Flags field.
                     * Low 8 bits = byte4, next 8 bits = status. */
                    fieldset_add_hex(
                        info->fieldset, "Flags", ((uint32_t)raw[7] << 8U) | raw[4], 16U);
                    fieldset_add_bytes(
                        info->fieldset, "Raw payload", raw, wanted_bytes * 2U);
                    fieldset_add_uint(
                        info->fieldset, "Raw payload bits", wanted_bits, 8U);
                    return true;
                }
            }
        }
        pos += 2U;
    }
    return false;
}

static bool rx18_q85_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    return rx18_abarth_decode_common(bits, numbytes, numbits, info, true);
}

static bool rx18_tg1c_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    return rx18_abarth_decode_common(bits, numbytes, numbits, info, false);
}

static void rx18_abarth_get_fields_common(ProtoViewFieldSet* fields) {
    const uint8_t default_id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", default_id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 2U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0U, 16U);
}

static void rx18_tg1c_get_fields(ProtoViewFieldSet* fields) {
    rx18_abarth_get_fields_common(fields);
}

static void rx18_q85_get_fields(ProtoViewFieldSet* fields) {
    rx18_abarth_get_fields_common(fields);
}

static void rx18_abarth_build_common(
    RawSamplesBuffer* samples,
    ProtoViewFieldSet* fields,
    bool q85) {
    const uint32_t te = 52U;
    const uint32_t bytes = q85 ? 12U : 9U;
    uint8_t raw[12] = {0};
    const bool have_raw = tpms_build_load_raw_payload(fields, raw, bytes, NULL) == bytes;
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        const uint32_t value = rx18_id32_from_field(id);
        raw[0] = (uint8_t)(value >> 24U);
        raw[1] = (uint8_t)(value >> 16U);
        raw[2] = (uint8_t)(value >> 8U);
        raw[3] = (uint8_t)value;
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        const uint32_t packed = flags ? (uint32_t)flags->uvalue : 0U;
        raw[4] = (uint8_t)(packed & 0xFFU);
        raw[7] = (uint8_t)((packed >> 8U) & 0xFFU);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        const float scale = q85 ? 3.0f : 1.38f;
        int32_t code = (int32_t)(pressure->fvalue / scale + 0.5f);
        raw[5] = (uint8_t)tpms_clamp_i32(code, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        const int32_t offset = q85 ? 55 : 50;
        raw[6] = (uint8_t)tpms_clamp_i32((int32_t)temperature->value + offset, 0, 255);
    }
    raw[8] = rx18_xor8(raw, 8U);
    if(q85) {
        raw[9] = 0x40U;
        const uint16_t crc = rx18_crc16_ccitt_false(raw, 10U);
        raw[10] = (uint8_t)crc;
        raw[11] = (uint8_t)(crc >> 8U);
    }

    /* rtl_433 first inverts the capture, therefore transmit the inverse of
     * AA AA A9 and of the Manchester symbols it decodes. */
    tpms_add_pattern(samples, "010101010101010101010110", te); /* 55 55 56 */
    rx18_add_manchester_bits(samples, raw, bytes * 8U, te, true);
}

static void rx18_tg1c_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    rx18_abarth_build_common(samples, fields, false);
}

static void rx18_q85_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    rx18_abarth_build_common(samples, fields, true);
}

ProtoViewDecoder Q85RX18TPMSDecoder = {
    .name = "Shenzhen Q85 TEST",
    .decode = rx18_q85_decode,
    .get_fields = rx18_q85_get_fields,
    .build_message = rx18_q85_build,
};

ProtoViewDecoder TG1CRX18TPMSDecoder = {
    .name = "VDO TG1C/Abarth TEST",
    .decode = rx18_tg1c_decode,
    .get_fields = rx18_tg1c_get_fields,
    .build_message = rx18_tg1c_build,
};

/* -------------------------------------------------------------------------- */
/* Mercedes-Benz Sprinter 4500                                                */
/* -------------------------------------------------------------------------- */

static bool rx18_mercedes_payload_valid(const uint8_t raw[10]) {
    if(raw[0] != 0x83U && raw[0] != 0xA3U) return false;
    return crc8(raw, 10U, 0xAAU, 0x2FU) == 0U;
}

static bool rx18_mercedes_try_manchester(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info,
    bool invert) {
    /* Decode candidate Manchester streams and look for the 12-bit logical
     * preamble 0x002 or 0xFF2. Trying both polarities also covers the second
     * observed preamble polarity. */
    for(uint32_t start = 0U; start + 184U <= numbits; start++) {
        uint8_t logical[12] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx18_manchester_decode(
            logical, sizeof(logical), bits, numbytes, numbits, start, 92U, invert, &consumed);
        if(decoded < 92U) continue;

        bool preamble_002 = true;
        bool preamble_ff2 = true;
        static const uint8_t p002[] = {0x00U, 0x20U};
        static const uint8_t pff2[] = {0xFFU, 0x20U};
        for(uint32_t i = 0U; i < 12U; i++) {
            const bool v = bitmap_get(logical, sizeof(logical), i);
            if(v != rx18_pattern_bit(p002, i)) preamble_002 = false;
            if(v != rx18_pattern_bit(pff2, i)) preamble_ff2 = false;
        }
        if(!preamble_002 && !preamble_ff2) continue;

        uint8_t raw[10] = {0};
        rx18_extract_bits(raw, sizeof(raw), logical, sizeof(logical), 12U, 80U);
        if(!rx18_mercedes_payload_valid(raw)) continue;

        const uint32_t id = ((uint32_t)raw[1] << 24U) | ((uint32_t)raw[2] << 16U) |
                            ((uint32_t)raw[3] << 8U) | raw[4];
        const float pressure_psi = (float)raw[5] / 2.75f;
        const int32_t temperature_c = (int32_t)raw[6] - 51;
        const uint32_t packed_flags = ((uint32_t)raw[0] << 16U) |
                                      ((uint32_t)raw[8] << 8U) | raw[7];
        if(pressure_psi > 150.0f || temperature_c < -80 || temperature_c > 180) continue;

        info->start_off = start;
        info->pulses_count = consumed;
        rx18_add_id32(info->fieldset, id);
        fieldset_add_float(info->fieldset, "Pressure psi", pressure_psi, 2U);
        fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
        /* Packed: bits 16..23 family/state, bits 8..15 flags2, bits 0..7
         * counter+flags1 byte. Keeping the complete bytes makes RX->TX exact. */
        fieldset_add_hex(info->fieldset, "Flags", packed_flags, 24U);
        fieldset_add_bytes(info->fieldset, "Raw payload", raw, 20U);
        fieldset_add_uint(info->fieldset, "Raw payload bits", 80U, 8U);
        return true;
    }
    return false;
}

static bool rx18_mercedes_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    return rx18_mercedes_try_manchester(bits, numbytes, numbits, info, false) ||
           rx18_mercedes_try_manchester(bits, numbytes, numbits, info, true);
}

static void rx18_mercedes_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t default_id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", default_id, 8U);
    fieldset_add_float(fields, "Pressure psi", 32.0f, 2U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    /* Default stationary family/state 0x83, flags2=0x6B, counter/flags1=0. */
    fieldset_add_hex(fields, "Flags", 0x836B00U, 24U);
}

static void rx18_mercedes_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 25U;
    uint8_t raw[10] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, raw, sizeof(raw), NULL) == sizeof(raw);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure psi");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");

    if(!have_raw) {
        raw[0] = 0x83U;
        raw[8] = 0x6BU;
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        const uint32_t value = rx18_id32_from_field(id);
        raw[1] = (uint8_t)(value >> 24U);
        raw[2] = (uint8_t)(value >> 16U);
        raw[3] = (uint8_t)(value >> 8U);
        raw[4] = (uint8_t)value;
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        int32_t code = (int32_t)(pressure->fvalue * 2.75f + 0.5f);
        raw[5] = (uint8_t)tpms_clamp_i32(code, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        raw[6] = (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 51, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        const uint32_t packed = flags ? (uint32_t)flags->uvalue : 0x836B00U;
        const uint8_t state = (uint8_t)((packed >> 16U) & 0xFFU);
        raw[0] = (state == 0xA3U) ? 0xA3U : 0x83U;
        raw[8] = (uint8_t)((packed >> 8U) & 0xFFU);
        raw[7] = (uint8_t)(packed & 0xFFU);
    }
    raw[9] = crc8(raw, 9U, 0xAAU, 0x2FU);

    uint8_t logical[12] = {0};
    /* 12-bit 0x002 preamble followed by the 80 payload bits. */
    logical[0] = 0x00U;
    logical[1] = 0x20U;
    rx18_insert_bits(logical, sizeof(logical), 12U, raw, sizeof(raw), 80U);
    rx18_add_manchester_bits(samples, logical, 92U, te, false);
}

ProtoViewDecoder MercedesSprinterRX18TPMSDecoder = {
    .name = "Mercedes Sprinter TEST",
    .decode = rx18_mercedes_decode,
    .get_fields = rx18_mercedes_get_fields,
    .build_message = rx18_mercedes_build,
};
