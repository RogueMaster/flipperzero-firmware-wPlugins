#pragma once

#include "../proto6_core.h"
#include "../../../helpers/tpms_edit_mask.h"

static inline void tpms_add_pattern(RawSamplesBuffer* samples, const char* pattern, uint32_t te) {
    for(const char* p = pattern; *p; p++) {
        raw_samples_add_or_update(samples, *p == '1', te);
    }
}

static inline void tpms_add_manchester(
    RawSamplesBuffer* samples,
    const uint8_t* data,
    size_t bytes,
    uint32_t te) {
    for(size_t bit = 0; bit < bytes * 8U; bit++) {
        if(bitmap_get((uint8_t*)data, bytes, bit)) {
            raw_samples_add_or_update(samples, true, te);
            raw_samples_add_or_update(samples, false, te);
        } else {
            raw_samples_add_or_update(samples, false, te);
            raw_samples_add_or_update(samples, true, te);
        }
    }
}

static inline void tpms_add_diff_manchester(
    RawSamplesBuffer* samples,
    const uint8_t* data,
    size_t bytes,
    uint32_t te,
    bool previous) {
    for(size_t bit = 0; bit < bytes * 8U; bit++) {
        const bool value = bitmap_get((uint8_t*)data, bytes, bit);
        const bool first = !previous;
        const bool second = value ? first : previous;
        raw_samples_add_or_update(samples, first, te);
        raw_samples_add_or_update(samples, second, te);
        previous = second;
    }
}

static inline int32_t tpms_clamp_i32(int32_t value, int32_t min, int32_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

static inline uint32_t tpms_clamp_u32(uint32_t value, uint32_t max) {
    return value > max ? max : value;
}

/* TPMS 2.3: retrieve the decoded source payload appended by tpms_encoder.c.
 * Builders use it as their starting frame and overwrite only editable fields. */
static inline size_t tpms_build_load_raw_payload(
    ProtoViewFieldSet* fields, uint8_t* dst, size_t dst_size, uint32_t* raw_bits) {
    if(raw_bits) *raw_bits = 0U;
    if(!fields || !dst || dst_size == 0U) return 0U;
    ProtoViewField* raw = proto6_field_find(fields, "Raw payload");
    if(!raw || raw->type != FieldTypeBytes || !raw->bytes) return 0U;
    size_t bytes = (raw->len + 1U) / 2U;
    if(bytes > dst_size) bytes = dst_size;
    memcpy(dst, raw->bytes, bytes);
    ProtoViewField* bits = proto6_field_find(fields, "Raw payload bits");
    if(raw_bits) *raw_bits = bits ? (uint32_t)bits->uvalue : (uint32_t)(bytes * 8U);
    return bytes;
}

static inline uint32_t tpms_build_get_edit_mask(ProtoViewFieldSet* fields) {
    ProtoViewField* mask = proto6_field_find(fields, "Edit mask");
    return mask ? (uint32_t)mask->uvalue : TPMS_EDIT_ALL;
}

static inline bool tpms_build_should_patch(
    ProtoViewFieldSet* fields, bool have_raw, uint32_t field_mask) {
    return !have_raw || ((tpms_build_get_edit_mask(fields) & field_mask) != 0U);
}
