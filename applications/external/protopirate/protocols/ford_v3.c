#include "ford_v3.h"
#include "../protopirate_app_i.h"
#include "protocols_common.h"
#include <string.h>

#define FORD_V3_TE_SHORT     240U
#define FORD_V3_TE_LONG      480U
#define FORD_V3_TE_DELTA     60U
#define FORD_V3_DATA_BITS    104U
#define FORD_V3_DATA_BYTES   13U
#define FORD_V3_PREAMBLE_MIN 30U

#define FORD_V3_BTN_LOCK   0x01U
#define FORD_V3_BTN_UNLOCK 0x02U

static const SubGhzBlockConst subghz_protocol_ford_v3_const = {
    .te_short = FORD_V3_TE_SHORT,
    .te_long = FORD_V3_TE_LONG,
    .te_delta = FORD_V3_TE_DELTA,
    .min_count_bit_for_found = FORD_V3_DATA_BITS,
};

typedef enum {
    FordV3DecoderStepReset = 0,
    FordV3DecoderStepPreamble = 1,
    FordV3DecoderStepData = 2,
} FordV3DecoderStep;

typedef struct SubGhzProtocolDecoderFordV3 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    ManchesterState manchester_state;
    uint8_t raw_bytes[FORD_V3_DATA_BYTES];
    uint8_t bit_count;
    uint16_t preamble_count;

    uint32_t serial;
    uint16_t counter;
} SubGhzProtocolDecoderFordV3;

static void ford_v3_reset_data(SubGhzProtocolDecoderFordV3* instance);
static void ford_v3_add_bit(SubGhzProtocolDecoderFordV3* instance, bool bit);
static void ford_v3_parse_fields(SubGhzProtocolDecoderFordV3* instance);
static void ford_v3_emit_if_ready(SubGhzProtocolDecoderFordV3* instance);
static const char* ford_v3_button_name(uint8_t btn);

static const char* ford_v3_button_name(uint8_t btn) {
    switch(btn) {
    case FORD_V3_BTN_LOCK:
        return "Lock";
    case FORD_V3_BTN_UNLOCK:
        return "Unlock";
    default:
        return "?";
    }
}

static void ford_v3_reset_data(SubGhzProtocolDecoderFordV3* instance) {
    memset(instance->raw_bytes, 0, sizeof(instance->raw_bytes));
    instance->bit_count = 0;
    instance->preamble_count = 0;
    manchester_advance(
        instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
}

static void ford_v3_add_bit(SubGhzProtocolDecoderFordV3* instance, bool bit) {
    if(instance->bit_count >= FORD_V3_DATA_BITS) {
        return;
    }

    const uint8_t byte_index = instance->bit_count / 8U;
    const uint8_t bit_in_byte = 7U - (instance->bit_count % 8U);
    if(bit) {
        instance->raw_bytes[byte_index] |= (uint8_t)(1U << bit_in_byte);
    }
    instance->bit_count++;
}

static void ford_v3_parse_fields(SubGhzProtocolDecoderFordV3* instance) {
    const uint8_t* b = instance->raw_bytes;

    instance->serial = ((uint32_t)b[1] << 24) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 8) |
                       (uint32_t)b[4];
    instance->counter =
        (uint16_t)((((uint16_t)(uint8_t)~b[7]) << 8) | (uint8_t)~b[8]);

    instance->generic.serial = instance->serial;
    instance->generic.btn = (b[6] & 0x01U) ? FORD_V3_BTN_UNLOCK : FORD_V3_BTN_LOCK;
    instance->generic.cnt = instance->counter;
}

static void ford_v3_emit_if_ready(SubGhzProtocolDecoderFordV3* instance) {
    if(instance->bit_count < FORD_V3_DATA_BITS) {
        return;
    }

    instance->generic.data_count_bit = FORD_V3_DATA_BITS;
    ford_v3_parse_fields(instance);

    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
}

void* subghz_protocol_decoder_ford_v3_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolDecoderFordV3* instance = calloc(1, sizeof(SubGhzProtocolDecoderFordV3));
    furi_check(instance);

    instance->base.protocol = &ford_protocol_v3;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void subghz_protocol_decoder_ford_v3_reset(void* context) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;
    instance->decoder.parser_step = FordV3DecoderStepReset;
    ford_v3_reset_data(instance);
}

void subghz_protocol_decoder_ford_v3_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;

    switch(instance->decoder.parser_step) {
    case FordV3DecoderStepReset:
        if(pp_is_short(duration, &subghz_protocol_ford_v3_const)) {
            ford_v3_reset_data(instance);
            instance->preamble_count = 1U;
            instance->decoder.parser_step = FordV3DecoderStepPreamble;
        }
        break;

    case FordV3DecoderStepPreamble:
        if(pp_is_short(duration, &subghz_protocol_ford_v3_const)) {
            instance->preamble_count++;
        } else if(
            instance->preamble_count >= FORD_V3_PREAMBLE_MIN &&
            pp_is_long(duration, &subghz_protocol_ford_v3_const)) {
            instance->manchester_state = ManchesterStateMid1;

            const ManchesterEvent event =
                level ? ManchesterEventLongHigh : ManchesterEventLongLow;

            bool data_bit = false;
            const bool valid = manchester_advance(
                instance->manchester_state, event, &instance->manchester_state, &data_bit);
            if(valid) {
                ford_v3_add_bit(instance, data_bit);
            }
            instance->decoder.parser_step = FordV3DecoderStepData;
        } else {
            instance->decoder.parser_step = FordV3DecoderStepReset;
        }
        break;

    case FordV3DecoderStepData: {
        if(!pp_is_short(duration, &subghz_protocol_ford_v3_const) &&
           !pp_is_long(duration, &subghz_protocol_ford_v3_const)) {
            ford_v3_emit_if_ready(instance);
            instance->decoder.parser_step = FordV3DecoderStepReset;

            if(pp_is_short(duration, &subghz_protocol_ford_v3_const)) {
                ford_v3_reset_data(instance);
                instance->preamble_count = 1U;
                instance->decoder.parser_step = FordV3DecoderStepPreamble;
            }
            break;
        }

        ManchesterEvent event;
        if(level) {
            event = pp_is_short(duration, &subghz_protocol_ford_v3_const) ? ManchesterEventShortHigh :
                                                                           ManchesterEventLongHigh;
        } else {
            event = pp_is_short(duration, &subghz_protocol_ford_v3_const) ? ManchesterEventShortLow :
                                                                           ManchesterEventLongLow;
        }

        bool data_bit = false;
        const bool valid = manchester_advance(
            instance->manchester_state, event, &instance->manchester_state, &data_bit);

        if(valid) {
            ford_v3_add_bit(instance, data_bit);
            if(instance->bit_count >= FORD_V3_DATA_BITS) {
                ford_v3_emit_if_ready(instance);
                instance->decoder.parser_step = FordV3DecoderStepReset;
            }
        }
        break;
    }
    }
}

uint8_t subghz_protocol_decoder_ford_v3_get_hash_data(void* context) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;
    uint8_t hash = 0;

    for(size_t i = 0; i < FORD_V3_DATA_BYTES; i++) {
        hash ^= instance->raw_bytes[i];
    }

    return hash;
}

SubGhzProtocolStatus subghz_protocol_decoder_ford_v3_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;

    instance->generic.data = ((uint64_t)instance->raw_bytes[0] << 56) |
                             ((uint64_t)instance->raw_bytes[1] << 48) |
                             ((uint64_t)instance->raw_bytes[2] << 40) |
                             ((uint64_t)instance->raw_bytes[3] << 32) |
                             ((uint64_t)instance->raw_bytes[4] << 24) |
                             ((uint64_t)instance->raw_bytes[5] << 16) |
                             ((uint64_t)instance->raw_bytes[6] << 8) |
                             (uint64_t)instance->raw_bytes[7];
    instance->generic.data_count_bit = FORD_V3_DATA_BITS;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if(ret == SubGhzProtocolStatusOk) {
        flipper_format_rewind(flipper_format);
        flipper_format_insert_or_update_hex(
            flipper_format, "Raw", instance->raw_bytes, FORD_V3_DATA_BYTES);

        pp_flipper_update_or_insert_u32(flipper_format, FF_SERIAL, instance->generic.serial);
        pp_flipper_update_or_insert_u32(flipper_format, FF_BTN, instance->generic.btn);
        pp_flipper_update_or_insert_u32(flipper_format, FF_CNT, instance->counter);
    }

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_ford_v3_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_ford_v3_const.min_count_bit_for_found);

    if(ret != SubGhzProtocolStatusOk) {
        return ret;
    }

    const uint64_t d = instance->generic.data;
    for(uint8_t i = 0; i < 8U; i++) {
        instance->raw_bytes[i] = (uint8_t)(d >> (56 - i * 8));
    }
    memset(&instance->raw_bytes[8], 0, FORD_V3_DATA_BYTES - 8U);

    flipper_format_rewind(flipper_format);
    flipper_format_read_hex(flipper_format, "Raw", instance->raw_bytes, FORD_V3_DATA_BYTES);

    instance->bit_count = FORD_V3_DATA_BITS;
    ford_v3_parse_fields(instance);

    return ret;
}

void subghz_protocol_decoder_ford_v3_get_string(void* context, FuriString* output) {
    furi_check(context);

    SubGhzProtocolDecoderFordV3* instance = context;
    const uint8_t* k = instance->raw_bytes;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n"
        "Sn:%08lX Btn:%02X %s\r\n"
        "Cnt:%04X Hop:%02X%02X%02X%02X\r\n",
        instance->generic.protocol_name,
        (int)instance->generic.data_count_bit,
        k[0],
        k[1],
        k[2],
        k[3],
        k[4],
        k[5],
        k[6],
        k[7],
        k[8],
        k[9],
        k[10],
        k[11],
        k[12],
        (unsigned long)instance->generic.serial,
        instance->generic.btn,
        ford_v3_button_name(instance->generic.btn),
        (unsigned)instance->counter,
        k[9],
        k[10],
        k[11],
        k[12]);
}

const SubGhzProtocolDecoder subghz_protocol_ford_v3_decoder = {
    .alloc = subghz_protocol_decoder_ford_v3_alloc,
    .free = pp_decoder_free_default,
    .feed = subghz_protocol_decoder_ford_v3_feed,
    .reset = subghz_protocol_decoder_ford_v3_reset,
    .get_hash_data = subghz_protocol_decoder_ford_v3_get_hash_data,
    .serialize = subghz_protocol_decoder_ford_v3_serialize,
    .deserialize = subghz_protocol_decoder_ford_v3_deserialize,
    .get_string = subghz_protocol_decoder_ford_v3_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_ford_v3_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol ford_protocol_v3 = {
    .name = FORD_PROTOCOL_V3_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_ford_v3_decoder,
    .encoder = &subghz_protocol_ford_v3_encoder,
};
