/*
 * Experimental RX/TX TPMS codecs ported from rtl_433 GPL-2.0 sources.
 * They are intentionally registered after the six proven ProtoView decoders.
 * TX builders create synthetic frames for receiver/plugin validation.
 */

#include "../proto6_core.h"
#include "tpms_build.h"

#define RX19_NOT_FOUND UINT32_MAX


static void rx19_add_manchester_bit(
    RawSamplesBuffer* samples,
    bool value,
    uint32_t te,
    bool invert) {
    /* rtl_433 convention used by the RX19 ports: 01 -> 1, 10 -> 0. */
    bool first = value ? false : true;
    bool second = !first;
    if(invert) {
        first = !first;
        second = !second;
    }
    raw_samples_add_or_update(samples, first, te);
    raw_samples_add_or_update(samples, second, te);
}

static void rx19_add_manchester_bits(
    RawSamplesBuffer* samples,
    const uint8_t* data,
    uint32_t bits,
    uint32_t te,
    bool invert) {
    const uint32_t bytes = (bits + 7U) / 8U;
    for(uint32_t bit = 0U; bit < bits; bit++) {
        rx19_add_manchester_bit(samples, bitmap_get((uint8_t*)data, bytes, bit), te, invert);
    }
}

static void rx19_add_diff_manchester_bits(
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

static void rx19_store_id32(uint8_t* dst, const ProtoViewField* field) {
    memset(dst, 0, 4U);
    if(!field || field->type != FieldTypeBytes || !field->bytes) return;
    uint32_t count = (field->len + 1U) / 2U;
    if(count > 4U) count = 4U;
    memcpy(dst + (4U - count), field->bytes, count);
}

static void rx19_store_id24(uint8_t* dst, const ProtoViewField* field) {
    memset(dst, 0, 3U);
    if(!field || field->type != FieldTypeBytes || !field->bytes) return;
    uint32_t count = (field->len + 1U) / 2U;
    if(count > 3U) count = 3U;
    memcpy(dst + (3U - count), field->bytes, count);
}

static bool rx19_get_bit(
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t pos,
    bool invert) {
    bool value = bitmap_get((uint8_t*)bits, numbytes, pos);
    return invert ? !value : value;
}

static bool rx19_pattern_bit(const uint8_t* pattern, uint32_t pos) {
    return ((pattern[pos / 8U] >> (7U - (pos & 7U))) & 1U) != 0U;
}

static uint32_t rx19_seek_pattern(
    const uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    uint32_t start,
    const uint8_t* pattern,
    uint32_t pattern_bits,
    bool invert) {
    if(!bits || !pattern || pattern_bits == 0U || start >= numbits) return RX19_NOT_FOUND;
    for(uint32_t pos = start; pos + pattern_bits <= numbits; pos++) {
        bool match = true;
        for(uint32_t i = 0U; i < pattern_bits; i++) {
            if(rx19_get_bit(bits, numbytes, pos + i, invert) != rx19_pattern_bit(pattern, i)) {
                match = false;
                break;
            }
        }
        if(match) return pos;
    }
    return RX19_NOT_FOUND;
}

/* Exact rtl_433 Manchester convention: 01 -> 1, 10 -> 0. */
static uint32_t rx19_manchester_decode(
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
        const bool a = rx19_get_bit(bits, numbytes, pos, invert);
        const bool b = rx19_get_bit(bits, numbytes, pos + 1U, invert);
        if(a == b) break;
        bitmap_set(out, outbytes, decoded++, (!a && b));
        pos += 2U;
    }
    if(consumed_raw) *consumed_raw = pos - start;
    return decoded;
}

/* Exact clock-alignment behaviour used by rtl_433 differential Manchester. */
static uint32_t rx19_diff_manchester_decode(
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
        bit1 = rx19_get_bit(bits, numbytes, pos++, invert);
        bit2 = rx19_get_bit(bits, numbytes, pos++, invert);
        const bool bit3 = rx19_get_bit(bits, numbytes, pos, invert);
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
        bit1 = rx19_get_bit(bits, numbytes, pos++, invert);
        if(bit1 == bit2) break;
        bit2 = rx19_get_bit(bits, numbytes, pos++, invert);
        bitmap_set(out, outbytes, decoded++, bit1 == bit2);
    }
    if(consumed_raw) *consumed_raw = pos - start;
    return decoded;
}

static void rx19_extract_bits(
    uint8_t* dst,
    uint32_t dstbytes,
    const uint8_t* src,
    uint32_t srcbytes,
    uint32_t start,
    uint32_t count) {
    memset(dst, 0, dstbytes);
    const uint32_t max = dstbytes * 8U;
    if(count > max) count = max;
    for(uint32_t i = 0U; i < count; i++) {
        bitmap_set(dst, dstbytes, i, bitmap_get((uint8_t*)src, srcbytes, start + i));
    }
}

static uint16_t rx19_crc16(
    const uint8_t* message,
    uint32_t count,
    uint16_t polynomial,
    uint16_t init) {
    uint16_t remainder = init;
    for(uint32_t byte = 0U; byte < count; byte++) {
        remainder ^= (uint16_t)message[byte] << 8U;
        for(uint32_t bit = 0U; bit < 8U; bit++) {
            remainder = (remainder & 0x8000U) ?
                            (uint16_t)((remainder << 1U) ^ polynomial) :
                            (uint16_t)(remainder << 1U);
        }
    }
    return remainder;
}

static void rx19_add_id32(ProtoViewFieldSet* fields, uint32_t id) {
    const uint8_t bytes[4] = {
        (uint8_t)(id >> 24U),
        (uint8_t)(id >> 16U),
        (uint8_t)(id >> 8U),
        (uint8_t)id,
    };
    fieldset_add_bytes(fields, "Tire ID", bytes, 8U);
}

static bool rx19_bmw_g3_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble[] = {0xCCU, 0xCDU};
    const uint32_t pos = rx19_seek_pattern(bits, numbytes, numbits, 0U, preamble, 16U, false);
    if(pos == RX19_NOT_FOUND || pos + 16U >= numbits) return false;

    uint8_t raw[11] = {0};
    uint32_t consumed = 0U;
    const uint32_t decoded = rx19_diff_manchester_decode(
        raw, sizeof(raw), bits, numbytes, numbits, pos + 16U, 88U, false, &consumed);
    if(decoded < 80U) return false;
    const bool gen2 = decoded < 88U;
    const uint32_t bytes = gen2 ? 10U : 11U;
    if(rx19_crc16(raw, bytes, 0x1021U, 0x0000U) != 0U) return false;

    const uint32_t id = ((uint32_t)raw[0] << 24U) | ((uint32_t)raw[1] << 16U) |
                        ((uint32_t)raw[2] << 8U) | raw[3];
    const float pressure_kpa = ((float)raw[4] - 43.0f) * 2.5f;
    const int32_t temperature_c = (int32_t)raw[5] - 40;
    if(pressure_kpa < -20.0f || pressure_kpa > 1200.0f || temperature_c < -80 ||
       temperature_c > 180) {
        return false;
    }

    info->start_off = pos;
    info->pulses_count = 16U + consumed;
    rx19_add_id32(info->fieldset, id);
    fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1U);
    fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
    fieldset_add_hex(info->fieldset, "Flags", ((uint16_t)raw[6] << 8U) | raw[7], 16U);
    fieldset_add_uint(info->fieldset, "Generation", gen2 ? 2U : 3U, 8U);
    fieldset_add_bytes(info->fieldset, "Raw payload", raw, bytes * 2U);
    fieldset_add_uint(info->fieldset, "Raw payload bits", bytes * 8U, 8U);
    return true;
}

static bool rx19_elantra_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble[] = {0x71U, 0x55U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(bits, numbytes, numbits, pos, preamble, 16U, false)) !=
          RX19_NOT_FOUND) {
        uint8_t raw[8] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 16U, 64U, false, &consumed);
        if(decoded >= 64U && crc8(raw, 8U, 0x00U, 0x07U) == 0U) {
            const uint32_t id = ((uint32_t)raw[2] << 24U) | ((uint32_t)raw[3] << 16U) |
                                ((uint32_t)raw[4] << 8U) | raw[5];
            const int32_t pressure_kpa = (int32_t)raw[0] + 60;
            const int32_t temperature_c = (int32_t)raw[1] - 50;
            if(pressure_kpa <= 700 && temperature_c >= -80 && temperature_c <= 180) {
                info->start_off = pos;
                info->pulses_count = 16U + consumed;
                rx19_add_id32(info->fieldset, id);
                fieldset_add_float(info->fieldset, "Pressure kpa", (float)pressure_kpa, 1U);
                fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                fieldset_add_uint(info->fieldset, "Battery", (raw[6] & 0x02U) ? 0U : 100U, 8U);
                fieldset_add_hex(info->fieldset, "Flags", raw[6], 8U);
                fieldset_add_bytes(info->fieldset, "Raw payload", raw, 8U * 2U);
                fieldset_add_uint(info->fieldset, "Raw payload bits", 64U, 8U);
                return true;
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_hyundai_vdo_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble_inverted[] = {0xAAU, 0xAAU, 0xAAU, 0xA9U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(
               bits, numbytes, numbits, pos, preamble_inverted, 32U, true)) != RX19_NOT_FOUND) {
        uint8_t raw[10] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 32U, 80U, true, &consumed);
        if(decoded >= 80U && crc8(raw, 9U, 0xAAU, 0x07U) == raw[9]) {
            const uint32_t id = ((uint32_t)raw[1] << 24U) | ((uint32_t)raw[2] << 16U) |
                                ((uint32_t)raw[3] << 8U) | raw[4];
            const float pressure_kpa = (float)raw[6] * 1.375f;
            const int32_t temperature_c = (int32_t)raw[7] - 50;
            if(pressure_kpa <= 700.0f && temperature_c >= -80 && temperature_c <= 180) {
                info->start_off = pos;
                info->pulses_count = 32U + consumed;
                rx19_add_id32(info->fieldset, id);
                fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1U);
                fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                fieldset_add_hex(info->fieldset, "Flags", raw[5] >> 4U, 4U);
                fieldset_add_uint(info->fieldset, "Repeat", raw[5] & 0x0FU, 4U);
                fieldset_add_uint(info->fieldset, "State", raw[0], 8U);
                fieldset_add_hex(info->fieldset, "Battery raw", raw[8], 8U);
                fieldset_add_bytes(info->fieldset, "Raw payload", raw, 10U * 2U);
                fieldset_add_uint(info->fieldset, "Raw payload bits", 80U, 8U);
                return true;
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_truck_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble_inverted[] = {0xAAU, 0xAAU, 0xA9U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(
               bits, numbytes, numbits, pos, preamble_inverted, 24U, true)) != RX19_NOT_FOUND) {
        uint8_t decoded_raw[10] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            decoded_raw,
            sizeof(decoded_raw),
            bits,
            numbytes,
            numbits,
            pos + 24U,
            76U,
            true,
            &consumed);
        if(decoded >= 76U) {
            uint8_t raw[9] = {0};
            rx19_extract_bits(raw, sizeof(raw), decoded_raw, sizeof(decoded_raw), 4U, 72U);
            if((raw[0] || raw[1] || raw[2] || raw[3]) && xor_bytes(raw, 9U, 0U) == 0U) {
                const uint32_t id = ((uint32_t)raw[0] << 24U) | ((uint32_t)raw[1] << 16U) |
                                    ((uint32_t)raw[2] << 8U) | raw[3];
                const uint16_t flags = raw[5] >> 4U;
                const uint16_t pressure_kpa = ((uint16_t)(raw[5] & 0x0FU) << 8U) | raw[6];
                const int32_t temperature_c = (int8_t)raw[7];
                /* Solar truck kits are a set of six wheel positions and the
                   published monitoring range is 0.1..12.0 bar. These two
                   sanity gates also reject the observed Citroen/VDO frame
                   that passed the XOR after a 4-bit shifted interpretation. */
                const bool wheel_valid = raw[4] >= 1U && raw[4] <= 6U;
                const bool pressure_valid = pressure_kpa >= 10U && pressure_kpa <= 1200U;
                if(wheel_valid && pressure_valid &&
                   temperature_c >= -127 && temperature_c <= 127) {
                    info->start_off = pos;
                    info->pulses_count = 24U + consumed;
                    rx19_add_id32(info->fieldset, id);
                    fieldset_add_float(info->fieldset, "Pressure kpa", (float)pressure_kpa, 1U);
                    fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                    fieldset_add_uint(
                        info->fieldset, "Battery", (flags & 0x03U) == 0x03U ? 100U : 0U, 8U);
                    fieldset_add_hex(info->fieldset, "Flags", flags, 4U);
                    fieldset_add_uint(info->fieldset, "Wheel", raw[4], 8U);
                    fieldset_add_hex(
                        info->fieldset, "State", (decoded_raw[0] >> 4U) & 0x0FU, 4U);
                    fieldset_add_bytes(
                        info->fieldset, "Raw payload", decoded_raw, 10U * 2U);
                    fieldset_add_uint(info->fieldset, "Raw payload bits", 76U, 8U);
                    return true;
                }
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_renault_0435r_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble_inverted[] = {0xAAU, 0xA9U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(
               bits, numbytes, numbits, pos, preamble_inverted, 16U, true)) != RX19_NOT_FOUND) {
        uint8_t raw[10] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 16U, 80U, true, &consumed);
        if(decoded >= 72U && xor_bytes(raw, 9U, 0U) == 0U) {
            const uint8_t tick = raw[8] & 0x7FU;
            const bool has_tick = (raw[8] >> 7U) != 0U;
            if(raw[8] && (!has_tick || tick > 30U)) {
                pos++;
                continue;
            }
            /* Observed production frames use the high flag nibble 0xC. This
             * extra gate reduces overlap with normal Renault 9-byte frames. */
            /* fix: Relearn frames (activated by LF) have 0xD as the nibble */
            if(((raw[3] & 0xF0U) != 0xC0U) && ((raw[3] & 0xF0U) != 0xD0U)) {
                pos++;
                continue;
            }
            const float pressure_kpa = (float)raw[4] / 0.75f;
            const int32_t temperature_c = (int32_t)raw[5] - 50;
            if(pressure_kpa <= 700.0f && temperature_c >= -80 && temperature_c <= 180) {
                const uint8_t id_bytes[3] = {raw[0], raw[1], raw[2]};
                info->start_off = pos;
                info->pulses_count = 16U + consumed;
                fieldset_add_bytes(info->fieldset, "Tire ID", id_bytes, 6U);
                fieldset_add_float(info->fieldset, "Pressure kpa", pressure_kpa, 1U);
                fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                fieldset_add_hex(info->fieldset, "Flags", raw[3], 8U);
                fieldset_add_uint(info->fieldset, "Tick", tick, 7U);
                fieldset_add_hex(info->fieldset, "Acceleration", raw[6], 8U);
                fieldset_add_bytes(info->fieldset, "Raw payload", raw, 9U * 2U);
                fieldset_add_uint(info->fieldset, "Raw payload bits", 72U, 8U);
                return true;
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_honda_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t marker[] = {0xDAU, 0xE3U, 0x54U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(bits, numbytes, numbits, pos, marker, 23U, false)) !=
          RX19_NOT_FOUND) {
        uint8_t raw[8] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 23U, 64U, false, &consumed);
        if(decoded >= 64U && crc8(raw, 7U, 0x00U, 0x07U) == raw[7]) {
            if(raw[0] > 0U && raw[0] < 50U) {
                pos++;
                continue;
            }
            const uint32_t id = ((uint32_t)raw[2] << 24U) | ((uint32_t)raw[3] << 16U) |
                                ((uint32_t)raw[4] << 8U) | raw[5];
            const float pressure_psi = (float)raw[0] * 0.2f;
            const int32_t temperature_c = (int32_t)raw[1] - 50;
            if(pressure_psi <= 150.0f && temperature_c >= -80 && temperature_c <= 180) {
                info->start_off = pos;
                info->pulses_count = 23U + consumed;
                rx19_add_id32(info->fieldset, id);
                fieldset_add_float(info->fieldset, "Pressure psi", pressure_psi, 1U);
                fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                fieldset_add_hex(info->fieldset, "Flags", raw[6], 8U);
                fieldset_add_bytes(info->fieldset, "Raw payload", raw, 8U * 2U);
                fieldset_add_uint(info->fieldset, "Raw payload bits", 64U, 8U);
                return true;
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_porsche_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble[] = {0x33U, 0x33U, 0x20U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(bits, numbytes, numbits, pos, preamble, 20U, false)) !=
          RX19_NOT_FOUND) {
        uint8_t raw[10] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_diff_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 20U, 80U, false, &consumed);
        if(decoded >= 80U && rx19_crc16(raw, 10U, 0x1021U, 0xFFFFU) == 0U) {
            const uint32_t id = ((uint32_t)raw[0] << 24U) | ((uint32_t)raw[1] << 16U) |
                                ((uint32_t)raw[2] << 8U) | raw[3];
            const int32_t pressure_kpa = (int32_t)raw[4] * 5 / 2 - 100;
            const int32_t temperature_c = (int32_t)raw[5] - 40;
            if(pressure_kpa >= -20 && pressure_kpa <= 700 && temperature_c >= -80 &&
               temperature_c <= 180) {
                info->start_off = pos;
                info->pulses_count = 20U + consumed;
                rx19_add_id32(info->fieldset, id);
                fieldset_add_float(info->fieldset, "Pressure kpa", (float)pressure_kpa, 1U);
                fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                fieldset_add_hex(
                    info->fieldset, "Flags", ((uint16_t)raw[6] << 8U) | raw[7], 16U);
                fieldset_add_bytes(info->fieldset, "Raw payload", raw, 10U * 2U);
                fieldset_add_uint(info->fieldset, "Raw payload bits", 80U, 8U);
                return true;
            }
        }
        pos++;
    }
    return false;
}

static bool rx19_kia_decode(
    uint8_t* bits,
    uint32_t numbytes,
    uint32_t numbits,
    ProtoViewMsgInfo* info) {
    static const uint8_t preamble[] = {0xEDU, 0x71U};
    uint32_t pos = 0U;
    while((pos = rx19_seek_pattern(bits, numbytes, numbits, pos, preamble, 16U, false)) !=
          RX19_NOT_FOUND) {
        uint8_t raw[9] = {0};
        uint32_t consumed = 0U;
        const uint32_t decoded = rx19_manchester_decode(
            raw, sizeof(raw), bits, numbytes, numbits, pos + 16U, 69U, false, &consumed);
        if(decoded >= 69U) {
            raw[8] &= 0xF8U;
            const uint8_t expected = crc8(raw, 8U, 0x76U, 0x07U);
            if(raw[8] == expected) {
                const uint8_t pressure_raw = (uint8_t)((raw[0] << 4U) | (raw[1] >> 4U));
                const uint8_t temperature_raw =
                    (uint8_t)((raw[1] << 4U) | (raw[2] >> 4U));
                const uint32_t id = ((uint32_t)raw[2] << 28U) | ((uint32_t)raw[3] << 20U) |
                                    ((uint32_t)raw[4] << 12U) | ((uint32_t)raw[5] << 4U) |
                                    (raw[6] >> 4U);
                const float pressure_psi = (float)pressure_raw / 5.0f;
                const int32_t temperature_c = (int32_t)temperature_raw - 50;
                if(pressure_psi <= 150.0f && temperature_c >= -80 && temperature_c <= 180) {
                    info->start_off = pos;
                    info->pulses_count = 16U + consumed;
                    rx19_add_id32(info->fieldset, id);
                    fieldset_add_float(info->fieldset, "Pressure psi", pressure_psi, 1U);
                    fieldset_add_int(info->fieldset, "Temperature C", temperature_c, 8U);
                    fieldset_add_hex(info->fieldset, "Flags", raw[0] >> 4U, 4U);
                    fieldset_add_hex(
                        info->fieldset,
                        "Unknown2",
                        ((uint16_t)(raw[6] & 0x0FU) << 8U) | raw[7],
                        12U);
                    fieldset_add_bytes(info->fieldset, "Raw payload", raw, 9U * 2U);
                    fieldset_add_uint(info->fieldset, "Raw payload bits", 69U, 8U);
                    return true;
                }
            }
        }
        pos++;
    }
    return false;
}



/* -------------------------------------------------------------------------
 * Experimental TX builders. They intentionally use conservative, plausible
 * default fields and the exact line-code/checksum expected by the RX ports.
 * ------------------------------------------------------------------------- */

static void rx19_bmw_g3_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xF802U, 16U);
    fieldset_add_uint(fields, "Generation", 3U, 8U);
}

static void rx19_bmw_g3_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[11] = {0};
    uint32_t raw_bits = 0U;
    const size_t raw_bytes =
        tpms_build_load_raw_payload(fields, data, sizeof(data), &raw_bits);
    const bool have_raw =
        (raw_bytes == 10U && raw_bits == 80U) ||
        (raw_bytes == 11U && raw_bits == 88U);

    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags_f = proto6_field_find(fields, "Flags");
    ProtoViewField* generation = proto6_field_find(fields, "Generation");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[4] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue / 2.5f + 43.5f), 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[5] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 40, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        const uint16_t flags = (uint16_t)flags_f->uvalue;
        data[6] = (uint8_t)(flags >> 8U);
        data[7] = (uint8_t)flags;
    }

    bool gen2;
    if(raw_bytes == 10U || raw_bits == 80U) gen2 = true;
    else if(raw_bytes == 11U || raw_bits == 88U) gen2 = false;
    else gen2 = generation && generation->uvalue == 2U;

    const uint32_t payload_bytes = gen2 ? 8U : 9U;
    if(!gen2 && !have_raw) data[8] = 0x03U;
    const uint16_t crc =
        rx19_crc16(data, payload_bytes, 0x1021U, 0x0000U);
    data[payload_bytes] = (uint8_t)(crc >> 8U);
    data[payload_bytes + 1U] = (uint8_t)crc;

    tpms_add_pattern(samples, "1100110011001101", te);
    rx19_add_diff_manchester_bits(
        samples, data, (payload_bytes + 2U) * 8U, te, true);
}

static void rx19_elantra_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xC0U, 8U);
}

static void rx19_elantra_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[8] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[0] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue + 0.5f) - 60, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[1] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data + 2U, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[6] = (uint8_t)flags->uvalue;
    }
    data[7] = crc8(data, 7U, 0x00U, 0x07U);

    tpms_add_pattern(samples, "0111000101010101", te);
    rx19_add_manchester_bits(samples, data, 64U, te, false);
}

static void rx19_hyundai_vdo_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0x0U, 4U);
    fieldset_add_uint(fields, "Repeat", 1U, 4U);
    fieldset_add_hex(fields, "State", 0x20U, 8U);
    fieldset_add_hex(fields, "Battery raw", 0x64U, 8U);
}

static void rx19_hyundai_vdo_build(
    RawSamplesBuffer* samples,
    ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[10] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");
    ProtoViewField* repeat = proto6_field_find(fields, "Repeat");
    ProtoViewField* state = proto6_field_find(fields, "State");
    ProtoViewField* battery_raw = proto6_field_find(fields, "Battery raw");

    if(!have_raw && state) data[0] = (uint8_t)state->uvalue;
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data + 1U, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[5] =
            (uint8_t)((data[5] & 0x0FU) | ((flags->uvalue & 0x0FU) << 4U));
    }
    if(!have_raw && repeat) {
        data[5] = (uint8_t)((data[5] & 0xF0U) | (repeat->uvalue & 0x0FU));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[6] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue / 1.375f + 0.5f), 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[7] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    if(!have_raw && battery_raw) data[8] = (uint8_t)battery_raw->uvalue;
    data[9] = crc8(data, 9U, 0xAAU, 0x07U);

    tpms_add_pattern(samples, "01010101010101010101010101010110", te);
    rx19_add_manchester_bits(samples, data, 80U, te, true);
}

static void rx19_truck_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xBU, 4U);
    fieldset_add_hex(fields, "Wheel", 0x01U, 8U);
    fieldset_add_hex(fields, "State", 0xAU, 4U);
}

static void rx19_truck_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[9] = {0};
    uint8_t packed[10] = {0};
    uint32_t raw_bits = 0U;
    const size_t raw_bytes =
        tpms_build_load_raw_payload(fields, packed, sizeof(packed), &raw_bits);
    const bool have_raw = raw_bytes == 10U && raw_bits == 76U;
    uint8_t state_value = 0U;
    if(have_raw) {
        state_value = (packed[0] >> 4U) & 0x0FU;
        rx19_extract_bits(data, sizeof(data), packed, sizeof(packed), 4U, 72U);
    }

    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure_f = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");
    ProtoViewField* wheel = proto6_field_find(fields, "Wheel");
    ProtoViewField* state = proto6_field_find(fields, "State");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data, id);
    }
    if(!have_raw && wheel) data[4] = (uint8_t)wheel->uvalue;
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        const uint16_t pressure = (uint16_t)tpms_clamp_i32(
            (int32_t)(pressure_f->fvalue + 0.5f), 0, 4095);
        data[5] =
            (uint8_t)((data[5] & 0xF0U) | ((pressure >> 8U) & 0x0FU));
        data[6] = (uint8_t)pressure;
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[5] =
            (uint8_t)((data[5] & 0x0FU) | ((flags->uvalue & 0x0FU) << 4U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[7] = (uint8_t)tpms_clamp_i32(
            (int32_t)temperature->value, -127, 127);
    }
    data[8] = xor_bytes(data, 8U, 0U);
    if(!have_raw && state) state_value = (uint8_t)(state->uvalue & 0x0FU);

    tpms_add_pattern(samples, "010101010101010101010110", te);
    for(int32_t bit = 3; bit >= 0; bit--) {
        rx19_add_manchester_bit(
            samples, ((state_value >> bit) & 1U) != 0U, te, true);
    }
    for(uint32_t bit = 0U; bit < 72U; bit++) {
        rx19_add_manchester_bit(
            samples, bitmap_get(data, sizeof(data), bit), te, true);
    }
}

static void rx19_renault_0435r_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[3] = {0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 6U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xC0U, 8U);
    fieldset_add_uint(fields, "Tick", 0U, 7U);
    fieldset_add_hex(fields, "Acceleration", 0x20U, 8U);
}

static void rx19_renault_0435r_build(
    RawSamplesBuffer* samples,
    ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[9] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");
    ProtoViewField* tick = proto6_field_find(fields, "Tick");
    ProtoViewField* acceleration = proto6_field_find(fields, "Acceleration");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id24(data, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[3] = (uint8_t)flags->uvalue;
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[4] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue * 0.75f + 0.5f), 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[5] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    if(!have_raw) {
        if(acceleration) data[6] = (uint8_t)acceleration->uvalue;
        if(tick) data[8] = (uint8_t)(0x80U | (tick->uvalue % 31U));
    }
    data[7] = 0U;
    data[7] = xor_bytes(data, 9U, 0U);

    tpms_add_pattern(samples, "0101010101010110", te);
    rx19_add_manchester_bits(samples, data, 72U, te, true);
}

static void rx19_honda_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure psi", 32.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xE1U, 8U);
}

static void rx19_honda_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 50U;
    uint8_t data[8] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure psi");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[0] = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure->fvalue * 5.0f + 0.5f), 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[1] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 50, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data + 2U, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[6] = (uint8_t)flags->uvalue;
    }
    data[7] = crc8(data, 7U, 0x00U, 0x07U);

    tpms_add_pattern(samples, "11011010111000110101010", te);
    rx19_add_manchester_bits(samples, data, 64U, te, false);
}

static void rx19_porsche_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure kpa", 220.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xBB02U, 16U);
}

static void rx19_porsche_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 52U;
    uint8_t data[10] = {0};
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), NULL) == sizeof(data);
    ProtoViewField* id = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure = proto6_field_find(fields, "Pressure kpa");
    ProtoViewField* temperature = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags_f = proto6_field_find(fields, "Flags");

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        rx19_store_id32(data, id);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        data[4] = (uint8_t)tpms_clamp_i32(
            (int32_t)((pressure->fvalue + 100.0f) / 2.5f + 0.5f), 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        data[5] =
            (uint8_t)tpms_clamp_i32((int32_t)temperature->value + 40, 0, 255);
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        const uint16_t flags = (uint16_t)flags_f->uvalue;
        data[6] = (uint8_t)(flags >> 8U);
        data[7] = (uint8_t)flags;
    }
    const uint16_t crc = rx19_crc16(data, 8U, 0x1021U, 0xFFFFU);
    data[8] = (uint8_t)(crc >> 8U);
    data[9] = (uint8_t)crc;

    tpms_add_pattern(samples, "00110011001100110010", te);
    rx19_add_diff_manchester_bits(samples, data, 80U, te, false);
}

static void rx19_kia_get_fields(ProtoViewFieldSet* fields) {
    const uint8_t id[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    fieldset_add_bytes(fields, "Tire ID", id, 8U);
    fieldset_add_float(fields, "Pressure psi", 32.0f, 1U);
    fieldset_add_int(fields, "Temperature C", 20, 8U);
    fieldset_add_hex(fields, "Flags", 0xFU, 4U);
    fieldset_add_hex(fields, "Unknown2", 0x000U, 12U);
}

static void rx19_kia_build(RawSamplesBuffer* samples, ProtoViewFieldSet* fields) {
    const uint32_t te = 50U;
    uint8_t data[9] = {0};
    uint32_t raw_bits = 0U;
    const bool have_raw =
        tpms_build_load_raw_payload(fields, data, sizeof(data), &raw_bits) ==
            sizeof(data) &&
        raw_bits == 69U;
    ProtoViewField* id_f = proto6_field_find(fields, "Tire ID");
    ProtoViewField* pressure_f = proto6_field_find(fields, "Pressure psi");
    ProtoViewField* temperature_f = proto6_field_find(fields, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fields, "Flags");
    ProtoViewField* unknown_f = proto6_field_find(fields, "Unknown2");

    uint8_t id_bytes[4];
    rx19_store_id32(id_bytes, id_f);
    const uint32_t id =
        ((uint32_t)id_bytes[0] << 24U) | ((uint32_t)id_bytes[1] << 16U) |
        ((uint32_t)id_bytes[2] << 8U) | id_bytes[3];

    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_PRESSURE)) {
        const uint8_t pressure = (uint8_t)tpms_clamp_i32(
            (int32_t)(pressure_f->fvalue * 5.0f + 0.5f), 0, 255);
        data[0] = (uint8_t)((data[0] & 0xF0U) | (pressure >> 4U));
        data[1] = (uint8_t)((data[1] & 0x0FU) | (pressure << 4U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_TEMPERATURE)) {
        const uint8_t temperature = (uint8_t)tpms_clamp_i32(
            (int32_t)temperature_f->value + 50, 0, 255);
        data[1] = (uint8_t)((data[1] & 0xF0U) | (temperature >> 4U));
        data[2] = (uint8_t)((data[2] & 0x0FU) | (temperature << 4U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_ID)) {
        data[2] = (uint8_t)((data[2] & 0xF0U) | ((id >> 28U) & 0x0FU));
        data[3] = (uint8_t)(id >> 20U);
        data[4] = (uint8_t)(id >> 12U);
        data[5] = (uint8_t)(id >> 4U);
        data[6] = (uint8_t)((data[6] & 0x0FU) | ((id & 0x0FU) << 4U));
    }
    if(tpms_build_should_patch(fields, have_raw, TPMS_EDIT_FLAGS)) {
        data[0] =
            (uint8_t)((data[0] & 0x0FU) | ((flags->uvalue & 0x0FU) << 4U));
    }

    const uint16_t base_unknown = have_raw ?
        (uint16_t)(((uint16_t)(data[6] & 0x0FU) << 8U) | data[7]) :
        (uint16_t)(unknown_f ? (unknown_f->uvalue & 0x0FFFU) : 0U);
    for(uint16_t offset = 0U; offset < 4096U; offset++) {
        const uint16_t unknown =
            (uint16_t)((base_unknown + offset) & 0x0FFFU);
        data[6] =
            (uint8_t)((data[6] & 0xF0U) | (uint8_t)(unknown >> 8U));
        data[7] = (uint8_t)unknown;
        const uint8_t crc = crc8(data, 8U, 0x76U, 0x07U);
        if((crc & 0x07U) == 0U) {
            data[8] = crc;
            break;
        }
    }

    tpms_add_pattern(samples, "1110110101110001", te);
    rx19_add_manchester_bits(samples, data, 69U, te, false);
}

ProtoViewDecoder BMWGen23RX19TPMSDecoder = {
    .name = "BMW Gen2/3 EXP",
    .decode = rx19_bmw_g3_decode,
    .get_fields = rx19_bmw_g3_get_fields,
    .build_message = rx19_bmw_g3_build,
};

ProtoViewDecoder Elantra2012RX19TPMSDecoder = {
    .name = "Elantra/Honda EXP",
    .decode = rx19_elantra_decode,
    .get_fields = rx19_elantra_get_fields,
    .build_message = rx19_elantra_build,
};

ProtoViewDecoder HyundaiVDORX19TPMSDecoder = {
    .name = "Hyundai VDO EXP",
    .decode = rx19_hyundai_vdo_decode,
    .get_fields = rx19_hyundai_vdo_get_fields,
    .build_message = rx19_hyundai_vdo_build,
};

ProtoViewDecoder TruckRX19TPMSDecoder = {
    .name = "Truck Solar EXP",
    .decode = rx19_truck_decode,
    .get_fields = rx19_truck_get_fields,
    .build_message = rx19_truck_build,
};

ProtoViewDecoder Renault0435RRX19TPMSDecoder = {
    .name = "Renault 0435R EXP",
    .decode = rx19_renault_0435r_decode,
    .get_fields = rx19_renault_0435r_get_fields,
    .build_message = rx19_renault_0435r_build,
};

ProtoViewDecoder HondaTRWRX19TPMSDecoder = {
    .name = "Honda TRW EXP",
    .decode = rx19_honda_decode,
    .get_fields = rx19_honda_get_fields,
    .build_message = rx19_honda_build,
};

ProtoViewDecoder PorscheRX19TPMSDecoder = {
    .name = "Porsche EXP",
    .decode = rx19_porsche_decode,
    .get_fields = rx19_porsche_get_fields,
    .build_message = rx19_porsche_build,
};

ProtoViewDecoder KiaRX19TPMSDecoder = {
    .name = "Kia EXP",
    .decode = rx19_kia_decode,
    .get_fields = rx19_kia_get_fields,
    .build_message = rx19_kia_build,
};
