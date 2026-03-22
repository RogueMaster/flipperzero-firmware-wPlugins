#include "mazda_v0.h"
#include <string.h>
// Original implementation by @lupettohf

#define MAZDA_PREAMBLE_MIN     13
#define MAZDA_COMPLETION_MIN   80
#define MAZDA_COMPLETION_MAX   105
#define MAZDA_DATA_BUFFER_SIZE 14

static const SubGhzBlockConst subghz_protocol_mazda_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 64,
};

typedef enum {
    MazdaDecoderStepReset = 0,
    MazdaDecoderStepPreambleSave,
    MazdaDecoderStepPreambleCheck,
    MazdaDecoderStepDataSave,
    MazdaDecoderStepDataCheck,
} MazdaDecoderStep;

struct SubGhzProtocolDecoderMazda {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t preamble_count;
    uint16_t bit_counter;
    uint8_t prev_state;
    uint8_t data_buffer[MAZDA_DATA_BUFFER_SIZE];
};

// ============================================================================
// Helpers
// ============================================================================

static uint8_t mazda_byte_parity(uint8_t value) {
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1;
}

static void mazda_xor_deobfuscate(uint8_t* data) {
    uint8_t parity = mazda_byte_parity(data[7]);

    if(parity) {
        uint8_t mask = data[6];
        for(uint8_t i = 0; i < 6; i++) {
            data[i] ^= mask;
        }
    } else {
        uint8_t mask = data[5];
        for(uint8_t i = 0; i < 5; i++) {
            data[i] ^= mask;
        }
        data[6] ^= mask;
    }

    uint8_t old5 = data[5];
    uint8_t old6 = data[6];
    data[5] = (old5 & 0xAAU) | (old6 & 0x55U);
    data[6] = (old5 & 0x55U) | (old6 & 0xAAU);
}

static void mazda_parse_data(SubGhzBlockGeneric* generic) {
    generic->serial = (uint32_t)(generic->data >> 32);
    generic->btn = (generic->data >> 24) & 0xFF;
    generic->cnt = (generic->data >> 8) & 0xFFFF;
}

static const char* mazda_get_btn_name(uint8_t btn) {
    switch(btn) {
    case 0x10:
        return "Lock";
    case 0x20:
        return "Unlock";
    case 0x40:
        return "Trunk";
    default:
        return "Unknown";
    }
}

static inline bool mazda_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_mazda_const.te_short) <
           subghz_protocol_mazda_const.te_delta;
}

static inline bool mazda_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, subghz_protocol_mazda_const.te_long) <
           subghz_protocol_mazda_const.te_delta;
}

static void mazda_collect_bit(SubGhzProtocolDecoderMazda* instance, uint8_t state_bit) {
    uint8_t byte_idx = instance->bit_counter >> 3;
    if(byte_idx < MAZDA_DATA_BUFFER_SIZE) {
        instance->data_buffer[byte_idx] <<= 1;
        if(state_bit == 0) {
            instance->data_buffer[byte_idx] |= 1;
        }
    }
    instance->bit_counter++;
}

static bool mazda_check_completion(SubGhzProtocolDecoderMazda* instance) {
    if(instance->bit_counter < MAZDA_COMPLETION_MIN ||
       instance->bit_counter > MAZDA_COMPLETION_MAX) {
        return false;
    }

    // Shift buffer by 1 byte (discard sync/header byte)
    uint8_t data[8];
    for(uint8_t i = 0; i < 8; i++) {
        data[i] = instance->data_buffer[i + 1];
    }

    mazda_xor_deobfuscate(data);

    uint8_t checksum = 0;
    for(uint8_t i = 0; i < 7; i++) {
        checksum += data[i];
    }
    if(checksum != data[7]) {
        return false;
    }

    uint64_t packed = 0;
    for(uint8_t i = 0; i < 8; i++) {
        packed = (packed << 8) | data[i];
    }

    instance->generic.data = packed;
    instance->generic.data_count_bit = 64;
    mazda_parse_data(&instance->generic);
    return true;
}

static bool mazda_process_pair(
    SubGhzProtocolDecoderMazda* instance,
    uint32_t dur_first,
    uint32_t dur_second) {
    bool first_short = mazda_is_short(dur_first);
    bool first_long = mazda_is_long(dur_first);
    bool second_short = mazda_is_short(dur_second);
    bool second_long = mazda_is_long(dur_second);

    if(first_long && second_short) {
        mazda_collect_bit(instance, 0);
        mazda_collect_bit(instance, 1);
        instance->prev_state = 1;
        return true;
    }

    if(first_short && second_long) {
        mazda_collect_bit(instance, 1);
        instance->prev_state = 0;
        return true;
    }

    if(first_short && second_short) {
        mazda_collect_bit(instance, instance->prev_state);
        return true;
    }

    if(first_long && second_long) {
        mazda_collect_bit(instance, 0);
        mazda_collect_bit(instance, 1);
        instance->prev_state = 0;
        return true;
    }

    return false;
}

const SubGhzProtocolDecoder subghz_protocol_mazda_decoder = {
    .alloc = subghz_protocol_decoder_mazda_alloc,
    .free = subghz_protocol_decoder_mazda_free,
    .feed = subghz_protocol_decoder_mazda_feed,
    .reset = subghz_protocol_decoder_mazda_reset,
    .get_hash_data = subghz_protocol_decoder_mazda_get_hash_data,
    .serialize = subghz_protocol_decoder_mazda_serialize,
    .deserialize = subghz_protocol_decoder_mazda_deserialize,
    .get_string = subghz_protocol_decoder_mazda_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_mazda_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol mazda_v0_protocol = {
    .name = MAZDA_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_mazda_decoder,
    .encoder = &subghz_protocol_mazda_encoder,
};

// ============================================================================
// Decoder
// ============================================================================

void* subghz_protocol_decoder_mazda_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderMazda* instance = calloc(1, sizeof(SubGhzProtocolDecoderMazda));
    furi_check(instance);
    instance->base.protocol = &mazda_v0_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_mazda_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    free(instance);
}

void subghz_protocol_decoder_mazda_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    instance->decoder.parser_step = MazdaDecoderStepReset;
    instance->preamble_count = 0;
    instance->bit_counter = 0;
    instance->prev_state = 0;
    instance->generic.data = 0;
    instance->generic.data_count_bit = 0;
    memset(instance->data_buffer, 0, sizeof(instance->data_buffer));
}

void subghz_protocol_decoder_mazda_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    UNUSED(level);
    SubGhzProtocolDecoderMazda* instance = context;

    switch(instance->decoder.parser_step) {
    case MazdaDecoderStepReset:
        if(mazda_is_short(duration)) {
            instance->decoder.te_last = duration;
            instance->preamble_count = 0;
            instance->decoder.parser_step = MazdaDecoderStepPreambleCheck;
        }
        break;

    case MazdaDecoderStepPreambleSave:
        instance->decoder.te_last = duration;
        instance->decoder.parser_step = MazdaDecoderStepPreambleCheck;
        break;

    case MazdaDecoderStepPreambleCheck:
        if(mazda_is_short(instance->decoder.te_last) && mazda_is_short(duration)) {
            instance->preamble_count++;
            instance->decoder.parser_step = MazdaDecoderStepPreambleSave;
        } else if(
            mazda_is_short(instance->decoder.te_last) && mazda_is_long(duration) &&
            instance->preamble_count >= MAZDA_PREAMBLE_MIN) {
            instance->bit_counter = 1;
            memset(instance->data_buffer, 0, sizeof(instance->data_buffer));
            mazda_collect_bit(instance, 1);
            instance->prev_state = 0;
            instance->decoder.parser_step = MazdaDecoderStepDataSave;
        } else {
            instance->decoder.parser_step = MazdaDecoderStepReset;
        }
        break;

    case MazdaDecoderStepDataSave:
        instance->decoder.te_last = duration;
        instance->decoder.parser_step = MazdaDecoderStepDataCheck;
        break;

    case MazdaDecoderStepDataCheck:
        if(mazda_process_pair(instance, instance->decoder.te_last, duration)) {
            instance->decoder.parser_step = MazdaDecoderStepDataSave;
        } else {
            if(mazda_check_completion(instance) && instance->base.callback) {
                instance->base.callback(&instance->base, instance->base.context);
            }
            instance->decoder.parser_step = MazdaDecoderStepReset;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_mazda_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    SubGhzBlockDecoder decoder = {
        .decode_data = instance->generic.data,
        .decode_count_bit = instance->generic.data_count_bit,
    };
    return subghz_protocol_blocks_get_hash_data(&decoder, (decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_mazda_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    return subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
}

SubGhzProtocolStatus
    subghz_protocol_decoder_mazda_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_mazda_const.min_count_bit_for_found);
    if(ret == SubGhzProtocolStatusOk) {
        mazda_parse_data(&instance->generic);
    }
    return ret;
}

void subghz_protocol_decoder_mazda_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderMazda* instance = context;
    mazda_parse_data(&instance->generic);

    uint8_t data[8];
    for(uint8_t i = 0; i < 8; i++) {
        data[i] = (instance->generic.data >> (56 - 8 * i)) & 0xFF;
    }

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%02X %02X %02X %02X %02X %02X %02X %02X\r\n"
        "Sn:%08lX Btn:%s\r\n"
        "Cnt:%04lX Chk:%02X\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        data[0],
        data[1],
        data[2],
        data[3],
        data[4],
        data[5],
        data[6],
        data[7],
        (uint32_t)instance->generic.serial,
        mazda_get_btn_name(instance->generic.btn),
        (uint32_t)instance->generic.cnt,
        data[7]);
}
