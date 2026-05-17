#include "land_rover_v0.h"
#include "protocols_common.h"

#include <string.h>

#define TAG "LandRoverV0"

static const SubGhzBlockConst subghz_protocol_land_rover_v0_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 81,
};

#define LAND_ROVER_V0_PREAMBLE_PAIRS     319U
#define LAND_ROVER_V0_MIN_PREAMBLE_PAIRS 64U
#define LAND_ROVER_V0_SYNC_US            750U
#define LAND_ROVER_V0_SYNC_DELTA_US      120U
#define LAND_ROVER_V0_UPLOAD_CAPACITY    1024U
#define LAND_ROVER_V0_GAP_US             50000U
_Static_assert(
    LAND_ROVER_V0_UPLOAD_CAPACITY <= PP_SHARED_UPLOAD_CAPACITY,
    "LAND_ROVER_V0_UPLOAD_CAPACITY exceeds shared upload slab");

#define LAND_ROVER_V0_BTN_UNKNOWN 0x00U
#define LAND_ROVER_V0_BTN_LOCK    0x02U
#define LAND_ROVER_V0_BTN_UNLOCK  0x04U

#define LAND_ROVER_V0_SIG_UNLOCK 0xA285E3UL
#define LAND_ROVER_V0_SIG_LOCK   0xC20363UL

#define LAND_ROVER_V0_FF_BTNSIG    "BtnSig"
#define LAND_ROVER_V0_FF_CHECK     "Check"
#define LAND_ROVER_V0_FF_TAIL      "Tail"
#define LAND_ROVER_V0_FF_EXTRA_BIT "ExtraBit"

typedef struct SubGhzProtocolDecoderLandRoverV0 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t preamble_count;
    uint8_t raw[10];
    uint8_t bit_count;
    bool extra_bit;
    bool previous_bit;
    bool boundary_pad_skipped;
    bool pending_short;

    uint64_t key;
    uint16_t tail;
    uint32_t command_signature;
    uint32_t serial;
    uint32_t count;
    uint8_t button;
    uint8_t check;
    bool check_ok;
    bool tail_ok;
} SubGhzProtocolDecoderLandRoverV0;

#ifdef ENABLE_EMULATE_FEATURE
typedef struct SubGhzProtocolEncoderLandRoverV0 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint64_t key;
    uint16_t tail;
    uint32_t command_signature;
    uint32_t serial;
    uint32_t count;
    uint8_t button;
    uint8_t check;
} SubGhzProtocolEncoderLandRoverV0;
#endif

typedef enum {
    LandRoverV0DecoderStepReset = 0,
    LandRoverV0DecoderStepPreambleLow,
    LandRoverV0DecoderStepPreambleHigh,
    LandRoverV0DecoderStepSyncLow,
    LandRoverV0DecoderStepData,
} LandRoverV0DecoderStep;

static uint8_t land_rover_v0_button_from_signature(uint32_t signature);
static const char* land_rover_v0_button_name(uint8_t button);
static uint8_t land_rover_v0_calculate_check(uint32_t count);
static bool land_rover_v0_calculate_tail_msb(uint32_t count);
static uint16_t land_rover_v0_calculate_tail(uint32_t count);
static void land_rover_v0_parse_key_fields(
    uint64_t key,
    uint32_t* signature,
    uint32_t* serial,
    uint32_t* count,
    uint8_t* button,
    uint8_t* check);
static bool land_rover_v0_validate_frame(
    uint64_t key,
    uint16_t tail,
    bool extra_bit,
    bool* check_ok,
    bool* tail_ok);
static bool land_rover_v0_add_decoded_bit(SubGhzProtocolDecoderLandRoverV0* instance, bool bit);
static bool land_rover_v0_process_transition(
    SubGhzProtocolDecoderLandRoverV0* instance,
    bool level,
    uint32_t duration);
static bool land_rover_v0_finish_frame(SubGhzProtocolDecoderLandRoverV0* instance);

#ifdef ENABLE_EMULATE_FEATURE
static bool land_rover_v0_encoder_add_level(
    SubGhzProtocolEncoderLandRoverV0* instance,
    size_t* index,
    bool level,
    uint32_t duration);
static bool land_rover_v0_encoder_add_bit(
    SubGhzProtocolEncoderLandRoverV0* instance,
    size_t* index,
    bool* previous_bit,
    bool bit);
static bool land_rover_v0_build_upload(SubGhzProtocolEncoderLandRoverV0* instance);
#endif

const SubGhzProtocolDecoder subghz_protocol_land_rover_v0_decoder = {
    .alloc = subghz_protocol_decoder_land_rover_v0_alloc,
    .free = subghz_protocol_decoder_land_rover_v0_free,
    .feed = subghz_protocol_decoder_land_rover_v0_feed,
    .reset = subghz_protocol_decoder_land_rover_v0_reset,
    .get_hash_data = subghz_protocol_decoder_land_rover_v0_get_hash_data,
    .serialize = subghz_protocol_decoder_land_rover_v0_serialize,
    .deserialize = subghz_protocol_decoder_land_rover_v0_deserialize,
    .get_string = subghz_protocol_decoder_land_rover_v0_get_string,
};

#ifdef ENABLE_EMULATE_FEATURE
const SubGhzProtocolEncoder subghz_protocol_land_rover_v0_encoder = {
    .alloc = subghz_protocol_encoder_land_rover_v0_alloc,
    .free = subghz_protocol_encoder_land_rover_v0_free,
    .deserialize = subghz_protocol_encoder_land_rover_v0_deserialize,
    .stop = subghz_protocol_encoder_land_rover_v0_stop,
    .yield = subghz_protocol_encoder_land_rover_v0_yield,
};
#else
const SubGhzProtocolEncoder subghz_protocol_land_rover_v0_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};
#endif

const SubGhzProtocol land_rover_v0_protocol = {
    .name = LAND_ROVER_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_land_rover_v0_decoder,
    .encoder = &subghz_protocol_land_rover_v0_encoder,
};

static bool land_rover_v0_is_short(uint32_t duration) {
    return pp_is_short(duration, &subghz_protocol_land_rover_v0_const);
}

static bool land_rover_v0_is_long(uint32_t duration) {
    return pp_is_long(duration, &subghz_protocol_land_rover_v0_const);
}

static bool land_rover_v0_is_sync(uint32_t duration) {
    return DURATION_DIFF(duration, LAND_ROVER_V0_SYNC_US) < LAND_ROVER_V0_SYNC_DELTA_US;
}

static uint8_t land_rover_v0_button_from_signature(uint32_t signature) {
    if(signature == LAND_ROVER_V0_SIG_UNLOCK) {
        return LAND_ROVER_V0_BTN_UNLOCK;
    } else if(signature == LAND_ROVER_V0_SIG_LOCK) {
        return LAND_ROVER_V0_BTN_LOCK;
    }
    return LAND_ROVER_V0_BTN_UNKNOWN;
}

static const char* land_rover_v0_button_name(uint8_t button) {
    switch(button) {
    case LAND_ROVER_V0_BTN_LOCK:
        return "Lock";
    case LAND_ROVER_V0_BTN_UNLOCK:
        return "Unlock";
    default:
        return "Unknown";
    }
}

static uint8_t land_rover_v0_calculate_check(uint32_t count) {
    const uint8_t c0 = ((count >> 1) ^ (count >> 2) ^ (count >> 3) ^ (count >> 4) ^ (count >> 6)) &
                       1U;
    const uint8_t c1 = ((count >> 0) ^ (count >> 2) ^ (count >> 3) ^ (count >> 4) ^ (count >> 5) ^
                        (count >> 6) ^ 1U) &
                       1U;
    const uint8_t c2 = ((count >> 1) ^ (count >> 3) ^ (count >> 4) ^ (count >> 5) ^ (count >> 6)) &
                       1U;

    return (uint8_t)(c0 | (c1 << 1) | (c2 << 2));
}

static bool land_rover_v0_calculate_tail_msb(uint32_t count) {
    const uint8_t tail = ((count >> 0) ^ (count >> 2) ^ (count >> 4) ^ (count >> 5)) & 1U;
    return tail != 0U;
}

static uint16_t land_rover_v0_calculate_tail(uint32_t count) {
    return land_rover_v0_calculate_tail_msb(count) ? 0xFFFFU : 0x7FFFU;
}

static void land_rover_v0_parse_key_fields(
    uint64_t key,
    uint32_t* signature,
    uint32_t* serial,
    uint32_t* count,
    uint8_t* button,
    uint8_t* check) {
    uint8_t key_bytes[8];
    pp_u64_to_bytes_be(key, key_bytes);

    const uint32_t sig = ((uint32_t)key_bytes[0] << 16) | ((uint32_t)key_bytes[1] << 8) |
                         key_bytes[2];
    const uint32_t sn = ((uint32_t)key_bytes[3] << 16) | ((uint32_t)key_bytes[4] << 8) |
                        key_bytes[5];
    const uint32_t cnt = ((uint32_t)key_bytes[6] << 1) | ((key_bytes[7] >> 7) & 1U);

    if(signature) *signature = sig;
    if(serial) *serial = sn;
    if(count) *count = cnt;
    if(button) *button = land_rover_v0_button_from_signature(sig);
    if(check) *check = key_bytes[7] & 0x07U;
}

static bool land_rover_v0_validate_frame(
    uint64_t key,
    uint16_t tail,
    bool extra_bit,
    bool* check_ok,
    bool* tail_ok) {
    uint8_t key_bytes[8];
    pp_u64_to_bytes_be(key, key_bytes);

    const uint32_t count = ((uint32_t)key_bytes[6] << 1) | ((key_bytes[7] >> 7) & 1U);
    const uint8_t expected_check = land_rover_v0_calculate_check(count);
    const uint16_t expected_tail = land_rover_v0_calculate_tail(count);

    const bool local_check_ok = ((key_bytes[7] & 0x78U) == 0U) &&
                                ((key_bytes[7] & 0x07U) == expected_check);
    const bool local_tail_ok = (tail == expected_tail) && extra_bit;

    if(check_ok) *check_ok = local_check_ok;
    if(tail_ok) *tail_ok = local_tail_ok;

    return local_check_ok && local_tail_ok;
}

static bool land_rover_v0_add_decoded_bit(SubGhzProtocolDecoderLandRoverV0* instance, bool bit) {
    if(instance->bit_count < 80U) {
        const uint8_t byte_index = instance->bit_count / 8U;
        const uint8_t bit_index = 7U - (instance->bit_count % 8U);
        if(bit) {
            instance->raw[byte_index] |= (uint8_t)(1U << bit_index);
        }
    } else if(instance->bit_count == 80U) {
        instance->extra_bit = bit;
    } else {
        return false;
    }

    instance->bit_count++;
    return true;
}

static bool land_rover_v0_finish_frame(SubGhzProtocolDecoderLandRoverV0* instance) {
    const uint64_t key = pp_bytes_to_u64_be(instance->raw);
    const uint16_t tail = ((uint16_t)instance->raw[8] << 8) | instance->raw[9];

    if(!land_rover_v0_validate_frame(
           key, tail, instance->extra_bit, &instance->check_ok, &instance->tail_ok)) {
        return false;
    }

    instance->key = key;
    instance->tail = tail;

    land_rover_v0_parse_key_fields(
        key,
        &instance->command_signature,
        &instance->serial,
        &instance->count,
        &instance->button,
        &instance->check);

    instance->generic.data = instance->key;
    instance->generic.data_count_bit = subghz_protocol_land_rover_v0_const.min_count_bit_for_found;
    instance->generic.serial = instance->serial;
    instance->generic.btn = instance->button;
    instance->generic.cnt = instance->count;

    return true;
}

static bool land_rover_v0_process_transition(
    SubGhzProtocolDecoderLandRoverV0* instance,
    bool level,
    uint32_t duration) {
    if(!instance->boundary_pad_skipped) {
        if(level && land_rover_v0_is_short(duration)) {
            instance->boundary_pad_skipped = true;
            return true;
        }
        instance->boundary_pad_skipped = true;
    }

    if(instance->pending_short) {
        if(!instance->previous_bit && !level && land_rover_v0_is_short(duration)) {
            instance->pending_short = false;
            return land_rover_v0_add_decoded_bit(instance, false);
        } else if(instance->previous_bit && level && land_rover_v0_is_short(duration)) {
            instance->pending_short = false;
            return land_rover_v0_add_decoded_bit(instance, true);
        }
        return false;
    }

    if(!instance->previous_bit) {
        if(level && land_rover_v0_is_long(duration)) {
            instance->previous_bit = true;
            return land_rover_v0_add_decoded_bit(instance, true);
        } else if(level && land_rover_v0_is_short(duration)) {
            instance->pending_short = true;
            return true;
        }
        return false;
    }

    if(!level && land_rover_v0_is_long(duration)) {
        instance->previous_bit = false;
        return land_rover_v0_add_decoded_bit(instance, false);
    } else if(!level && land_rover_v0_is_short(duration)) {
        instance->pending_short = true;
        return true;
    }

    return false;
}

#ifdef ENABLE_EMULATE_FEATURE
static bool land_rover_v0_encoder_add_level(
    SubGhzProtocolEncoderLandRoverV0* instance,
    size_t* index,
    bool level,
    uint32_t duration) {
    if(*index >= LAND_ROVER_V0_UPLOAD_CAPACITY) {
        return false;
    }
    instance->encoder.upload[(*index)++] = level_duration_make(level, duration);
    return true;
}

static bool land_rover_v0_encoder_add_bit(
    SubGhzProtocolEncoderLandRoverV0* instance,
    size_t* index,
    bool* previous_bit,
    bool bit) {
    const uint32_t te_short = subghz_protocol_land_rover_v0_const.te_short;
    const uint32_t te_long = subghz_protocol_land_rover_v0_const.te_long;

    if(!*previous_bit && !bit) {
        if(!land_rover_v0_encoder_add_level(instance, index, true, te_short) ||
           !land_rover_v0_encoder_add_level(instance, index, false, te_short)) {
            return false;
        }
    } else if(!*previous_bit && bit) {
        if(!land_rover_v0_encoder_add_level(instance, index, true, te_long)) {
            return false;
        }
    } else if(*previous_bit && !bit) {
        if(!land_rover_v0_encoder_add_level(instance, index, false, te_long)) {
            return false;
        }
    } else {
        if(!land_rover_v0_encoder_add_level(instance, index, false, te_short) ||
           !land_rover_v0_encoder_add_level(instance, index, true, te_short)) {
            return false;
        }
    }

    *previous_bit = bit;
    return true;
}

static bool land_rover_v0_build_upload(SubGhzProtocolEncoderLandRoverV0* instance) {
    furi_check(instance);

    size_t index = 0;
    const uint32_t te_short = subghz_protocol_land_rover_v0_const.te_short;

    uint8_t key_bytes[8];
    pp_u64_to_bytes_be(instance->key, key_bytes);

    for(uint16_t i = 0; i < LAND_ROVER_V0_PREAMBLE_PAIRS; i++) {
        if(!land_rover_v0_encoder_add_level(instance, &index, true, te_short) ||
           !land_rover_v0_encoder_add_level(instance, &index, false, te_short)) {
            return false;
        }
    }

    if(!land_rover_v0_encoder_add_level(instance, &index, true, LAND_ROVER_V0_SYNC_US) ||
       !land_rover_v0_encoder_add_level(instance, &index, false, LAND_ROVER_V0_SYNC_US) ||
       !land_rover_v0_encoder_add_level(instance, &index, true, te_short)) {
        return false;
    }

    bool previous_bit = true;
    if(!land_rover_v0_encoder_add_bit(instance, &index, &previous_bit, false)) {
        return false;
    }

    for(uint8_t bit_index = 2; bit_index < 64; bit_index++) {
        const uint8_t byte_index = bit_index / 8U;
        const uint8_t bit_in_byte = 7U - (bit_index % 8U);
        const bool bit = (key_bytes[byte_index] >> bit_in_byte) & 1U;
        if(!land_rover_v0_encoder_add_bit(instance, &index, &previous_bit, bit)) {
            return false;
        }
    }

    instance->tail = land_rover_v0_calculate_tail(instance->count);
    for(uint8_t bit_index = 0; bit_index < 16; bit_index++) {
        const bool bit = (instance->tail >> (15U - bit_index)) & 1U;
        if(!land_rover_v0_encoder_add_bit(instance, &index, &previous_bit, bit)) {
            return false;
        }
    }

    if(!land_rover_v0_encoder_add_bit(instance, &index, &previous_bit, true)) {
        return false;
    }

    if(!land_rover_v0_encoder_add_level(instance, &index, false, LAND_ROVER_V0_GAP_US)) {
        return false;
    }

    instance->encoder.front = 0;
    instance->encoder.size_upload = index;
    return true;
}
#endif

void* subghz_protocol_decoder_land_rover_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderLandRoverV0* instance =
        calloc(1, sizeof(SubGhzProtocolDecoderLandRoverV0));
    furi_check(instance);

    instance->base.protocol = &land_rover_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_land_rover_v0_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;
    free(instance);
}

void subghz_protocol_decoder_land_rover_v0_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    instance->decoder.parser_step = LandRoverV0DecoderStepReset;
    instance->decoder.te_last = 0;
    instance->preamble_count = 0;
    memset(instance->raw, 0, sizeof(instance->raw));
    instance->bit_count = 0;
    instance->extra_bit = false;
    instance->previous_bit = true;
    instance->boundary_pad_skipped = false;
    instance->pending_short = false;
}

void subghz_protocol_decoder_land_rover_v0_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    switch(instance->decoder.parser_step) {
    case LandRoverV0DecoderStepReset:
        if(level && land_rover_v0_is_short(duration)) {
            instance->preamble_count = 0;
            instance->decoder.parser_step = LandRoverV0DecoderStepPreambleLow;
        }
        break;

    case LandRoverV0DecoderStepPreambleLow:
        if(!level && land_rover_v0_is_short(duration)) {
            instance->preamble_count++;
            instance->decoder.parser_step = LandRoverV0DecoderStepPreambleHigh;
        } else {
            instance->decoder.parser_step = LandRoverV0DecoderStepReset;
        }
        break;

    case LandRoverV0DecoderStepPreambleHigh:
        if(level && land_rover_v0_is_short(duration)) {
            instance->decoder.parser_step = LandRoverV0DecoderStepPreambleLow;
        } else if(
            level && land_rover_v0_is_sync(duration) &&
            instance->preamble_count >= LAND_ROVER_V0_MIN_PREAMBLE_PAIRS) {
            instance->decoder.parser_step = LandRoverV0DecoderStepSyncLow;
        } else {
            instance->decoder.parser_step = LandRoverV0DecoderStepReset;
        }
        break;

    case LandRoverV0DecoderStepSyncLow:
        if(!level && land_rover_v0_is_sync(duration)) {
            memset(instance->raw, 0, sizeof(instance->raw));
            instance->bit_count = 0;
            instance->extra_bit = false;
            instance->previous_bit = true;
            instance->boundary_pad_skipped = false;
            instance->pending_short = false;
            land_rover_v0_add_decoded_bit(instance, true);
            instance->decoder.parser_step = LandRoverV0DecoderStepData;
        } else {
            instance->decoder.parser_step = LandRoverV0DecoderStepReset;
        }
        break;

    case LandRoverV0DecoderStepData:
        if(!land_rover_v0_process_transition(instance, level, duration)) {
            instance->decoder.parser_step = LandRoverV0DecoderStepReset;
            break;
        }

        if(instance->bit_count == subghz_protocol_land_rover_v0_const.min_count_bit_for_found) {
            if(land_rover_v0_finish_frame(instance) && instance->base.callback) {
                instance->base.callback(&instance->base, instance->base.context);
            }
            instance->decoder.parser_step = LandRoverV0DecoderStepReset;
        }
        break;
    }

    instance->decoder.te_last = duration;
}

uint8_t subghz_protocol_decoder_land_rover_v0_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    SubGhzBlockDecoder decoder = {
        .decode_data = instance->key,
        .decode_count_bit = 64,
    };
    uint8_t hash = subghz_protocol_blocks_get_hash_data(&decoder, 9);
    hash ^= (uint8_t)(instance->tail >> 8);
    hash ^= (uint8_t)instance->tail;
    hash ^= instance->extra_bit ? 1U : 0U;
    return hash;
}

SubGhzProtocolStatus subghz_protocol_decoder_land_rover_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if(ret == SubGhzProtocolStatusOk) {
        uint8_t key_bytes[8];
        pp_u64_to_bytes_be(instance->key, key_bytes);
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(flipper_format, FF_KEY, key_bytes, sizeof(key_bytes));
        pp_flipper_update_or_insert_u32(flipper_format, FF_SERIAL, instance->serial);
        pp_flipper_update_or_insert_u32(flipper_format, FF_BTN, instance->button);
        pp_flipper_update_or_insert_u32(
            flipper_format, LAND_ROVER_V0_FF_BTNSIG, instance->command_signature);
        pp_flipper_update_or_insert_u32(flipper_format, FF_CNT, instance->count);
        pp_flipper_update_or_insert_u32(flipper_format, LAND_ROVER_V0_FF_CHECK, instance->check);
        pp_flipper_update_or_insert_u32(flipper_format, LAND_ROVER_V0_FF_TAIL, instance->tail);
        pp_flipper_update_or_insert_u32(
            flipper_format, LAND_ROVER_V0_FF_EXTRA_BIT, instance->extra_bit ? 1U : 0U);
    }

    return ret;
}

SubGhzProtocolStatus subghz_protocol_decoder_land_rover_v0_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_land_rover_v0_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        uint8_t key_bytes[8] = {0};
        bool have_key = false;

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_hex(flipper_format, FF_KEY, key_bytes, sizeof(key_bytes))) {
            instance->key = pp_bytes_to_u64_be(key_bytes);
            have_key = true;
        }

        if(!have_key) {
            instance->key = instance->generic.data;
            pp_u64_to_bytes_be(instance->key, key_bytes);
        }

        uint32_t temp = 0;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, LAND_ROVER_V0_FF_TAIL, &temp, 1)) {
            instance->tail = temp & 0xFFFFU;
        } else {
            const uint32_t count = ((uint32_t)key_bytes[6] << 1) | ((key_bytes[7] >> 7) & 1U);
            instance->tail = land_rover_v0_calculate_tail(count);
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, LAND_ROVER_V0_FF_EXTRA_BIT, &temp, 1)) {
            instance->extra_bit = (temp & 1U) != 0;
        } else {
            instance->extra_bit = true;
        }

        land_rover_v0_validate_frame(
            instance->key,
            instance->tail,
            instance->extra_bit,
            &instance->check_ok,
            &instance->tail_ok);
        land_rover_v0_parse_key_fields(
            instance->key,
            &instance->command_signature,
            &instance->serial,
            &instance->count,
            &instance->button,
            &instance->check);

        instance->generic.data = instance->key;
        instance->generic.data_count_bit =
            subghz_protocol_land_rover_v0_const.min_count_bit_for_found;
        instance->generic.serial = instance->serial;
        instance->generic.btn = instance->button;
        instance->generic.cnt = instance->count;
    }

    return ret;
}

void subghz_protocol_decoder_land_rover_v0_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderLandRoverV0* instance = context;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Sn:%06lX  Btn:%02X - %s\r\n"
        "BtnSig:%06lX\r\n"
        "Cnt:%05lX  Chk:%02X [%s]  Tail:%05lX [%s]\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (unsigned long long)instance->key,
        (unsigned long)instance->serial,
        instance->button,
        land_rover_v0_button_name(instance->button),
        (unsigned long)instance->command_signature,
        (unsigned long)instance->count,
        instance->check,
        instance->check_ok ? "OK" : "BAD",
        (unsigned long)(((instance->tail >> 15) & 1U) ? 0x1FFFFUL : 0x0FFFFUL),
        instance->tail_ok ? "OK" : "BAD");
}

#ifdef ENABLE_EMULATE_FEATURE

static uint32_t land_rover_v0_signature_from_button(uint8_t button) {
    switch(button) {
    case LAND_ROVER_V0_BTN_LOCK:
        return LAND_ROVER_V0_SIG_LOCK;
    case LAND_ROVER_V0_BTN_UNLOCK:
        return LAND_ROVER_V0_SIG_UNLOCK;
    default:
        return 0;
    }
}

static uint64_t land_rover_v0_build_key(uint32_t signature, uint32_t serial, uint32_t count) {
    uint8_t key_bytes[8] = {0};
    key_bytes[0] = (uint8_t)((signature >> 16) & 0xFFU);
    key_bytes[1] = (uint8_t)((signature >> 8) & 0xFFU);
    key_bytes[2] = (uint8_t)(signature & 0xFFU);
    key_bytes[3] = (uint8_t)((serial >> 16) & 0xFFU);
    key_bytes[4] = (uint8_t)((serial >> 8) & 0xFFU);
    key_bytes[5] = (uint8_t)(serial & 0xFFU);
    key_bytes[6] = (uint8_t)((count >> 1) & 0xFFU);

    const bool counter_lsb = (count & 1U) != 0;
    const uint8_t check = land_rover_v0_calculate_check(count);
    key_bytes[7] = (counter_lsb ? 0x80U : 0x00U) | check;

    return pp_bytes_to_u64_be(key_bytes);
}

void* subghz_protocol_encoder_land_rover_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderLandRoverV0* instance =
        calloc(1, sizeof(SubGhzProtocolEncoderLandRoverV0));
    furi_check(instance);

    instance->base.protocol = &land_rover_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 0;
    instance->encoder.front = 0;
    instance->encoder.is_running = false;
    pp_encoder_buffer_ensure(instance, LAND_ROVER_V0_UPLOAD_CAPACITY);

    return instance;
}

void subghz_protocol_encoder_land_rover_v0_free(void* context) {
    furi_check(context);
    pp_encoder_free(context);
}

SubGhzProtocolStatus subghz_protocol_encoder_land_rover_v0_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderLandRoverV0* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    instance->encoder.repeat = 10;

    do {
        if(pp_verify_protocol_name(flipper_format, instance->base.protocol->name) !=
           SubGhzProtocolStatusOk) {
            break;
        }

        SubGhzProtocolStatus load_status = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic,
            flipper_format,
            subghz_protocol_land_rover_v0_const.min_count_bit_for_found);
        if(load_status != SubGhzProtocolStatusOk) {
            break;
        }

        instance->serial = instance->generic.serial & 0xFFFFFFU;
        instance->button = instance->generic.btn;
        instance->count = instance->generic.cnt & 0x1FFU;

        uint8_t key_bytes[8] = {0};
        bool have_key = false;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_hex(flipper_format, FF_KEY, key_bytes, sizeof(key_bytes))) {
            instance->key = pp_bytes_to_u64_be(key_bytes);
            have_key = true;
        }

        if(have_key) {
            land_rover_v0_parse_key_fields(
                instance->key,
                &instance->command_signature,
                &instance->serial,
                &instance->count,
                &instance->button,
                &instance->check);
        }

        uint32_t u32 = 0;
        bool have_button = false;

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, FF_SERIAL, &u32, 1)) {
            instance->serial = u32 & 0xFFFFFFU;
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, FF_CNT, &u32, 1)) {
            instance->count = u32 & 0x1FFU;
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, FF_BTN, &u32, 1)) {
            instance->button = (uint8_t)u32;
            have_button = true;
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, LAND_ROVER_V0_FF_BTNSIG, &u32, 1)) {
            instance->command_signature = u32 & 0xFFFFFFU;
        }

        if(have_button) {
            const uint32_t signature = land_rover_v0_signature_from_button(instance->button);
            if(signature != 0U) {
                instance->command_signature = signature;
            }
        }

        if(instance->command_signature == 0U) {
            break;
        }

        instance->key = land_rover_v0_build_key(
            instance->command_signature, instance->serial, instance->count);

        pp_u64_to_bytes_be(instance->key, key_bytes);
        instance->tail = land_rover_v0_calculate_tail(instance->count);

        land_rover_v0_parse_key_fields(
            instance->key,
            &instance->command_signature,
            &instance->serial,
            &instance->count,
            &instance->button,
            &instance->check);

        instance->generic.data = instance->key;
        instance->generic.data_count_bit =
            subghz_protocol_land_rover_v0_const.min_count_bit_for_found;
        instance->generic.serial = instance->serial;
        instance->generic.btn = instance->button;
        instance->generic.cnt = instance->count;

        flipper_format_rewind(flipper_format);
        instance->encoder.repeat = pp_encoder_read_repeat(flipper_format, 10);

        if(!land_rover_v0_build_upload(instance) || instance->encoder.size_upload == 0U) {
            break;
        }

        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(flipper_format, FF_KEY, key_bytes, sizeof(key_bytes));
        pp_flipper_update_or_insert_u32(flipper_format, FF_SERIAL, instance->serial);
        pp_flipper_update_or_insert_u32(flipper_format, FF_BTN, instance->button);
        pp_flipper_update_or_insert_u32(
            flipper_format, LAND_ROVER_V0_FF_BTNSIG, instance->command_signature);
        pp_flipper_update_or_insert_u32(flipper_format, FF_CNT, instance->count);
        pp_flipper_update_or_insert_u32(flipper_format, LAND_ROVER_V0_FF_CHECK, instance->check);
        pp_flipper_update_or_insert_u32(flipper_format, LAND_ROVER_V0_FF_TAIL, instance->tail);
        pp_flipper_update_or_insert_u32(flipper_format, LAND_ROVER_V0_FF_EXTRA_BIT, 1U);

        instance->encoder.is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void subghz_protocol_encoder_land_rover_v0_stop(void* context) {
    pp_encoder_stop(context);
}

LevelDuration subghz_protocol_encoder_land_rover_v0_yield(void* context) {
    return pp_encoder_yield(context);
}
#endif
