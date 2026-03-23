#include "mitsubishi_v0.h"
#include <string.h>

// Original implementation by @lupettohf

#define MITSUBISHI_BIT_COUNT  96
#define MITSUBISHI_DATA_BYTES 12

static const SubGhzBlockConst subghz_protocol_mitsubishi_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 80,
};

typedef enum {
    MitsubishiDecoderStepReset = 0,
    MitsubishiDecoderStepDataSave,
    MitsubishiDecoderStepDataCheck,
} MitsubishiDecoderStep;

struct SubGhzProtocolDecoderMitsubishi {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint8_t decoder_state;
    uint16_t bit_count;
    uint8_t decode_data[MITSUBISHI_DATA_BYTES];
};

static void mitsubishi_unscramble_payload(uint8_t* payload) {
    for(uint8_t i = 0; i < 8; i++) {
        payload[i] = (uint8_t)~payload[i];
    }

    uint16_t counter = ((uint16_t)payload[4] << 8) | payload[5];
    uint8_t hi = (counter >> 8) & 0xFF;
    uint8_t lo = counter & 0xFF;
    uint8_t mask1 = (hi & 0xAAU) | (lo & 0x55U);
    uint8_t mask2 = (lo & 0xAAU) | (hi & 0x55U);
    uint8_t mask3 = mask1 ^ mask2;

    for(uint8_t i = 0; i < 5; i++) {
        payload[i] ^= mask3;
    }
}

static inline bool mitsubishi_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_mitsubishi_const.te_short) <
           subghz_protocol_mitsubishi_const.te_delta;
}

static inline bool mitsubishi_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_mitsubishi_const.te_long) <
           subghz_protocol_mitsubishi_const.te_delta;
}

static void mitsubishi_reset_payload(SubGhzProtocolDecoderMitsubishi* instance) {
    instance->bit_count = 0;
    memset(instance->decode_data, 0, sizeof(instance->decode_data));
}

static bool mitsubishi_collect_pair(
    SubGhzProtocolDecoderMitsubishi* instance,
    uint32_t high,
    uint32_t low) {
    bool bit_value;

    if(mitsubishi_is_short(high) && mitsubishi_is_long(low)) {
        bit_value = true;
    } else if(mitsubishi_is_long(high) && mitsubishi_is_short(low)) {
        bit_value = false;
    } else {
        return false;
    }

    uint16_t bit_index = instance->bit_count;
    if(bit_index < MITSUBISHI_BIT_COUNT) {
        if(bit_value) {
            uint8_t byte_index = bit_index >> 3;
            uint8_t bit_position = 7 - (bit_index & 0x07);
            instance->decode_data[byte_index] |= (1U << bit_position);
        }
        instance->bit_count++;
    }

    return true;
}

static void mitsubishi_publish_frame(SubGhzProtocolDecoderMitsubishi* instance) {
    uint8_t payload[MITSUBISHI_DATA_BYTES];
    memcpy(payload, instance->decode_data, sizeof(payload));
    mitsubishi_unscramble_payload(payload);

    instance->generic.data_count_bit = instance->bit_count;
    instance->generic.serial = ((uint32_t)payload[0] << 24) | ((uint32_t)payload[1] << 16) |
                               ((uint32_t)payload[2] << 8) | payload[3];
    instance->generic.cnt = ((uint16_t)payload[4] << 8) | payload[5];
    instance->generic.btn = payload[6];

    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
}

const SubGhzProtocolDecoder subghz_protocol_mitsubishi_decoder = {
    .alloc = subghz_protocol_decoder_mitsubishi_alloc,
    .free = subghz_protocol_decoder_mitsubishi_free,
    .feed = subghz_protocol_decoder_mitsubishi_feed,
    .reset = subghz_protocol_decoder_mitsubishi_reset,
    .get_hash_data = subghz_protocol_decoder_mitsubishi_get_hash_data,
    .serialize = subghz_protocol_decoder_mitsubishi_serialize,
    .deserialize = subghz_protocol_decoder_mitsubishi_deserialize,
    .get_string = subghz_protocol_decoder_mitsubishi_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_mitsubishi_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol mitsubishi_v0_protocol = {
    .name = MITSUBISHI_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_868 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_mitsubishi_decoder,
    .encoder = &subghz_protocol_mitsubishi_encoder,
};

void* subghz_protocol_decoder_mitsubishi_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderMitsubishi* instance = calloc(1, sizeof(SubGhzProtocolDecoderMitsubishi));
    furi_check(instance);
    instance->base.protocol = &mitsubishi_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_mitsubishi_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;
    free(instance);
}

void subghz_protocol_decoder_mitsubishi_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;
    instance->decoder_state = MitsubishiDecoderStepReset;
    instance->decoder.te_last = 0;
    instance->generic.data_count_bit = 0;
    mitsubishi_reset_payload(instance);
}

void subghz_protocol_decoder_mitsubishi_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;

    switch(instance->decoder_state) {
    case MitsubishiDecoderStepReset:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder_state = MitsubishiDecoderStepDataCheck;
        }
        break;

    case MitsubishiDecoderStepDataSave:
        if(level) {
            instance->decoder.te_last = duration;
            instance->decoder_state = MitsubishiDecoderStepDataCheck;
        } else {
            instance->decoder_state = MitsubishiDecoderStepReset;
            mitsubishi_reset_payload(instance);
        }
        break;

    case MitsubishiDecoderStepDataCheck:
        if(!level) {
            if(mitsubishi_collect_pair(instance, instance->decoder.te_last, duration)) {
                if(instance->bit_count >= MITSUBISHI_BIT_COUNT) {
                    mitsubishi_publish_frame(instance);
                    mitsubishi_reset_payload(instance);
                    instance->decoder_state = MitsubishiDecoderStepReset;
                } else {
                    instance->decoder_state = MitsubishiDecoderStepDataSave;
                }
            } else {
                mitsubishi_reset_payload(instance);
                instance->decoder_state = MitsubishiDecoderStepReset;
            }
        } else {
            instance->decoder.te_last = duration;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_mitsubishi_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;
    uint8_t hash = 0;
    for(size_t i = 0; i < sizeof(instance->decode_data); i++) {
        hash ^= instance->decode_data[i];
    }
    return hash;
}

SubGhzProtocolStatus subghz_protocol_decoder_mitsubishi_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;
    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(ret == SubGhzProtocolStatusOk) {
        flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1);
        flipper_format_write_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1);
        uint32_t btn = instance->generic.btn;
        flipper_format_write_uint32(flipper_format, "Btn", &btn, 1);
    }
    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_mitsubishi_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;
    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_mitsubishi_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        flipper_format_read_uint32(flipper_format, "Serial", &instance->generic.serial, 1);
        flipper_format_read_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1);
        uint32_t btn = 0;
        flipper_format_read_uint32(flipper_format, "Btn", &btn, 1);
        instance->generic.btn = (uint8_t)btn;
    }

    return ret;
}

void subghz_protocol_decoder_mitsubishi_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderMitsubishi* instance = context;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Sn:%08lX Cnt:%04lX\r\n"
        "Btn:%02X\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->generic.serial,
        instance->generic.cnt,
        instance->generic.btn);
}
