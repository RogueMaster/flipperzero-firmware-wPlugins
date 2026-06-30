#pragma once

#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>
#include <lib/subghz/blocks/const.h>
#include <lib/subghz/blocks/decoder.h>
#include <lib/subghz/blocks/encoder.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/blocks/math.h>
#include <lib/subghz/protocols/base.h>
#include <lib/toolbox/manchester_decoder.h>

extern const char FF_BIT[];
extern const char FF_KEY[];
extern const char FF_SERIAL[];
extern const char FF_BTN[];
extern const char FF_CNT[];
extern const char FF_REPEAT[];
extern const char FF_PROTOCOL[];
extern const char FF_PRESET[];
extern const char FF_FREQUENCY[];
extern const char FF_MANUFACTURE[];
extern const char FF_TYPE[];

bool pp_preset_name_is_custom_marker(const char* preset_name);

const char* pp_get_short_preset_name(const char* preset_name);
bool pp_parse_hex_u64_strict(const char* str, uint64_t* out_key);
bool pp_flipper_read_hex_u64(FlipperFormat* flipper_format, const char* key, uint64_t* out_key);
void pp_flipper_update_or_insert_u32(
    FlipperFormat* flipper_format,
    const char* key,
    uint32_t value);

SubGhzProtocolStatus pp_verify_protocol_name(FlipperFormat* ff, const char* expected_name);

#define PP_FIELD_SERIAL 0x01U
#define PP_FIELD_BTN    0x02U
#define PP_FIELD_CNT    0x04U
#define PP_FIELD_TYPE   0x08U

SubGhzProtocolStatus pp_encoder_read_bit(
    FlipperFormat* ff,
    const uint16_t* allowed_bits,
    size_t allowed_bits_count,
    uint32_t* out_bit);

void pp_encoder_read_fields(
    FlipperFormat* ff,
    uint32_t* serial_out,
    uint32_t* btn_out,
    uint32_t* cnt_out,
    uint32_t* type_out);

uint32_t pp_encoder_read_repeat(FlipperFormat* ff, uint32_t default_repeat);

SubGhzProtocolStatus pp_serialize_fields(
    FlipperFormat* ff,
    uint32_t field_mask,
    uint32_t serial,
    uint32_t btn,
    uint32_t cnt,
    uint32_t type);

SubGhzProtocolStatus
    pp_write_display(FlipperFormat* ff, const char* protocol_name, const char* suffix);

static inline size_t pp_emit(LevelDuration* up, size_t i, size_t cap, bool level, uint32_t us) {
    if(i < cap) up[i++] = level_duration_make(level, us);
    return i;
}

size_t pp_emit_merge(LevelDuration* up, size_t i, size_t cap, bool level, uint32_t us);

static inline size_t
    pp_emit_manchester_bit(LevelDuration* up, size_t i, size_t cap, bool bit_value, uint32_t te) {
    i = pp_emit(up, i, cap, bit_value, te);
    i = pp_emit(up, i, cap, !bit_value, te);
    return i;
}

size_t
    pp_emit_byte_manchester(LevelDuration* up, size_t i, size_t cap, uint8_t value, uint32_t te);

size_t
    pp_emit_short_pairs(LevelDuration* up, size_t i, size_t cap, uint32_t te, size_t pair_count);

uint8_t pp_reverse_bits8(uint8_t value);
void pp_u64_to_bytes_be(uint64_t data, uint8_t bytes[8]);
uint64_t pp_bytes_to_u64_be(const uint8_t bytes[8]);

static inline bool pp_is_short(uint32_t duration, const SubGhzBlockConst* t) {
    return DURATION_DIFF(duration, t->te_short) < t->te_delta;
}

static inline bool pp_is_long(uint32_t duration, const SubGhzBlockConst* t) {
    return DURATION_DIFF(duration, t->te_long) < t->te_delta;
}

static inline ManchesterEvent
    pp_manchester_event(uint32_t duration, bool level, const SubGhzBlockConst* t) {
    if(DURATION_DIFF(duration, t->te_short) < t->te_delta) {
        return level ? ManchesterEventShortLow : ManchesterEventShortHigh;
    }
    if(DURATION_DIFF(duration, t->te_long) < t->te_delta) {
        return level ? ManchesterEventLongLow : ManchesterEventLongHigh;
    }
    return ManchesterEventReset;
}

typedef struct {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
} ProtoPirateDecoderHeader;

uint8_t pp_decoder_hash_blocks(void* context);

void pp_decoder_free_default(void* context);

typedef struct {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
} ProtoPirateEncoderHeader;

void pp_encoder_free(void* context);
void pp_encoder_stop(void* context);
LevelDuration pp_encoder_yield(void* context);

#define PP_SHARED_UPLOAD_CAPACITY 2048U

void pp_encoder_buffer_ensure(void* context, size_t capacity);

LevelDuration* pp_shared_upload_buffer(void);
size_t pp_shared_upload_capacity(void);
void pp_shared_upload_release(void);
