#include "porsche_touareg.h"
#include <string.h>

// Original implementation by @lupettohf

#define PORSCHE_CAYENNE_BIT_COUNT 64
#define PC_TE_SYNC                3370U
#define PC_TE_GAP                 5930U
#define PC_SYNC_MIN               15

static const SubGhzBlockConst subghz_protocol_porsche_cayenne_const = {
    .te_short = 1680,
    .te_long = 3370,
    .te_delta = 500,
    .min_count_bit_for_found = PORSCHE_CAYENNE_BIT_COUNT,
};

typedef enum {
    PorscheCayenneDecoderStepReset = 0,
    PorscheCayenneDecoderStepSync,
    PorscheCayenneDecoderStepGapHigh,
    PorscheCayenneDecoderStepGapLow,
    PorscheCayenneDecoderStepData,
} PorscheCayenneDecoderStep;

struct SubGhzProtocolDecoderPorscheCayenne {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t sync_count;
    uint64_t raw_data;
    uint8_t bit_count;
};

static void porsche_cayenne_compute_frame(
    uint32_t serial24,
    uint8_t btn,
    uint16_t counter,
    uint8_t frame_type,
    uint8_t* pkt) {
    uint8_t b0 = (uint8_t)((btn << 4) | (frame_type & 0x07));
    uint8_t b1 = (serial24 >> 16) & 0xFF;
    uint8_t b2 = (serial24 >> 8) & 0xFF;
    uint8_t b3 = serial24 & 0xFF;

    uint16_t cnt = counter + 1;
    uint8_t cnt_lo = cnt & 0xFF;
    uint8_t cnt_hi = (cnt >> 8) & 0xFF;

    uint8_t r_h = b3;
    uint8_t r_m = b1;
    uint8_t r_l = b2;

#define ROTATE24(rh, rm, rl)                 \
    do {                                     \
        uint8_t _ch = ((rh) >> 7) & 1U;      \
        uint8_t _cm = ((rm) >> 7) & 1U;      \
        uint8_t _cl = ((rl) >> 7) & 1U;      \
        (rh) = (uint8_t)(((rh) << 1) | _cm); \
        (rm) = (uint8_t)(((rm) << 1) | _cl); \
        (rl) = (uint8_t)(((rl) << 1) | _ch); \
    } while(0)

    for(uint8_t i = 0; i < 4; i++) {
        ROTATE24(r_h, r_m, r_l);
    }
    for(uint16_t i = 0; i < cnt_lo; i++) {
        ROTATE24(r_h, r_m, r_l);
    }

#undef ROTATE24

    uint8_t a9a = r_h ^ b0;

    uint8_t nb9b_p1 = (uint8_t)((~cnt_lo << 2) & 0xFC) ^ r_m;
    uint8_t nb9b_p2 = (uint8_t)((~cnt_hi << 2) & 0xFC) ^ r_m;
    uint8_t nb9b_p3 = (uint8_t)((~cnt_hi >> 6) & 0x03) ^ r_m;
    uint8_t a9b = (nb9b_p1 & 0xCC) | (nb9b_p2 & 0x30) | (nb9b_p3 & 0x03);

    uint8_t nb9c_p1 = (uint8_t)((~cnt_lo >> 2) & 0x3F) ^ r_l;
    uint8_t nb9c_p2 = (uint8_t)((~cnt_hi & 0x03) << 6) ^ r_l;
    uint8_t nb9c_p3 = (uint8_t)((~cnt_hi >> 2) & 0x3F) ^ r_l;
    uint8_t a9c = (nb9c_p1 & 0x33) | (nb9c_p2 & 0xC0) | (nb9c_p3 & 0x0C);

    pkt[0] = b0;
    pkt[1] = b1;
    pkt[2] = b2;
    pkt[3] = b3;
    pkt[4] = (uint8_t)(((a9a >> 2) & 0x3F) | ((~cnt_lo & 0x03U) << 6));
    pkt[5] = (uint8_t)((~cnt_lo & 0xC0U) | ((a9a & 0x03U) << 4) | (a9b & 0x0CU) |
                       ((~cnt_lo >> 2) & 0x03U));
    pkt[6] = (uint8_t)(((a9b & 0x03U) << 6) | ((a9c >> 2) & 0x3CU) | ((~cnt_lo >> 4) & 0x03U));
    pkt[7] = (uint8_t)(((a9b >> 4) & 0x0FU) | ((a9c & 0x0FU) << 4));
}

static void porsche_cayenne_parse_data(SubGhzProtocolDecoderPorscheCayenne* instance) {
    uint8_t pkt[8];
    uint64_t raw = instance->generic.data;

    for(int8_t i = 7; i >= 0; i--) {
        pkt[i] = (uint8_t)(raw & 0xFF);
        raw >>= 8;
    }

    instance->generic.serial = ((uint32_t)pkt[1] << 16) | ((uint32_t)pkt[2] << 8) | pkt[3];
    instance->generic.btn = (uint8_t)(pkt[0] >> 4);
    instance->generic.cnt = 0;

    uint8_t frame_type = pkt[0] & 0x07;
    uint8_t try_pkt[8];
    for(uint16_t try_cnt = 1; try_cnt <= 256; try_cnt++) {
        porsche_cayenne_compute_frame(
            instance->generic.serial,
            instance->generic.btn,
            (uint16_t)(try_cnt - 1),
            frame_type,
            try_pkt);
        if(try_pkt[4] == pkt[4] && try_pkt[5] == pkt[5] && try_pkt[6] == pkt[6] &&
           try_pkt[7] == pkt[7]) {
            instance->generic.cnt = try_cnt;
            break;
        }
    }
}

static void porsche_cayenne_publish_frame(SubGhzProtocolDecoderPorscheCayenne* instance) {
    instance->generic.data = instance->raw_data;
    instance->generic.data_count_bit = PORSCHE_CAYENNE_BIT_COUNT;
    porsche_cayenne_parse_data(instance);

    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
}

const SubGhzProtocolDecoder subghz_protocol_porsche_cayenne_decoder = {
    .alloc = subghz_protocol_decoder_porsche_cayenne_alloc,
    .free = subghz_protocol_decoder_porsche_cayenne_free,
    .feed = subghz_protocol_decoder_porsche_cayenne_feed,
    .reset = subghz_protocol_decoder_porsche_cayenne_reset,
    .get_hash_data = subghz_protocol_decoder_porsche_cayenne_get_hash_data,
    .serialize = subghz_protocol_decoder_porsche_cayenne_serialize,
    .deserialize = subghz_protocol_decoder_porsche_cayenne_deserialize,
    .get_string = subghz_protocol_decoder_porsche_cayenne_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_porsche_cayenne_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol porsche_touareg_protocol = {
    .name = PORSCHE_CAYENNE_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_868 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_porsche_cayenne_decoder,
    .encoder = &subghz_protocol_porsche_cayenne_encoder,
};

void* subghz_protocol_decoder_porsche_cayenne_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderPorscheCayenne* instance =
        calloc(1, sizeof(SubGhzProtocolDecoderPorscheCayenne));
    furi_check(instance);
    instance->base.protocol = &porsche_touareg_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_porsche_cayenne_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;
    free(instance);
}

void subghz_protocol_decoder_porsche_cayenne_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;
    instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
    instance->decoder.te_last = 0;
    instance->sync_count = 0;
    instance->raw_data = 0;
    instance->bit_count = 0;
    instance->generic.data = 0;
    instance->generic.data_count_bit = 0;
}

void subghz_protocol_decoder_porsche_cayenne_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;

    const uint32_t te_short = subghz_protocol_porsche_cayenne_const.te_short;
    const uint32_t te_long = subghz_protocol_porsche_cayenne_const.te_long;
    const uint32_t te_delta = subghz_protocol_porsche_cayenne_const.te_delta;

    switch(instance->decoder.parser_step) {
    case PorscheCayenneDecoderStepReset:
        if(!level && DURATION_DIFF(duration, PC_TE_SYNC) < te_delta) {
            instance->sync_count = 1;
            instance->decoder.parser_step = PorscheCayenneDecoderStepSync;
        }
        break;

    case PorscheCayenneDecoderStepSync:
        if(level) {
            if(DURATION_DIFF(duration, PC_TE_SYNC) < te_delta) {
                // keep collecting sync pairs
            } else if(
                instance->sync_count >= PC_SYNC_MIN &&
                DURATION_DIFF(duration, PC_TE_GAP) < te_delta) {
                instance->decoder.parser_step = PorscheCayenneDecoderStepGapLow;
            } else {
                instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
            }
        } else {
            if(DURATION_DIFF(duration, PC_TE_SYNC) < te_delta) {
                instance->sync_count++;
            } else if(
                instance->sync_count >= PC_SYNC_MIN &&
                DURATION_DIFF(duration, PC_TE_GAP) < te_delta) {
                instance->decoder.parser_step = PorscheCayenneDecoderStepGapHigh;
            } else {
                instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
            }
        }
        break;

    case PorscheCayenneDecoderStepGapHigh:
        if(level && DURATION_DIFF(duration, PC_TE_GAP) < te_delta) {
            instance->raw_data = 0;
            instance->bit_count = 0;
            instance->decoder.parser_step = PorscheCayenneDecoderStepData;
        } else {
            instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
        }
        break;

    case PorscheCayenneDecoderStepGapLow:
        if(!level && DURATION_DIFF(duration, PC_TE_GAP) < te_delta) {
            instance->raw_data = 0;
            instance->bit_count = 0;
            instance->decoder.parser_step = PorscheCayenneDecoderStepData;
        } else {
            instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
        }
        break;

    case PorscheCayenneDecoderStepData:
        if(level) {
            bool bit_value = false;
            if(DURATION_DIFF(instance->decoder.te_last, te_short) < te_delta &&
               DURATION_DIFF(duration, te_long) < te_delta) {
                bit_value = false;
            } else if(
                DURATION_DIFF(instance->decoder.te_last, te_long) < te_delta &&
                DURATION_DIFF(duration, te_short) < te_delta) {
                bit_value = true;
            } else {
                instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
                break;
            }

            instance->raw_data = (instance->raw_data << 1) | (bit_value ? 1U : 0U);
            instance->bit_count++;

            if(instance->bit_count >= PORSCHE_CAYENNE_BIT_COUNT) {
                porsche_cayenne_publish_frame(instance);
                instance->decoder.parser_step = PorscheCayenneDecoderStepReset;
            }
        } else {
            instance->decoder.te_last = duration;
        }
        break;
    }
}

uint8_t subghz_protocol_decoder_porsche_cayenne_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;
    SubGhzBlockDecoder decoder = {
        .decode_data = instance->generic.data,
        .decode_count_bit = instance->generic.data_count_bit,
    };
    return subghz_protocol_blocks_get_hash_data(&decoder, (decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_porsche_cayenne_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(ret == SubGhzProtocolStatusOk) {
        uint32_t serial = instance->generic.serial & 0xFFFFFF;
        flipper_format_write_uint32(flipper_format, "Serial", &serial, 1);
        uint32_t cnt = instance->generic.cnt;
        flipper_format_write_uint32(flipper_format, "Cnt", &cnt, 1);
        uint32_t btn = instance->generic.btn;
        flipper_format_write_uint32(flipper_format, "Btn", &btn, 1);
    }

    return ret;
}

SubGhzProtocolStatus subghz_protocol_decoder_porsche_cayenne_deserialize(
    void* context,
    FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_porsche_cayenne_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        porsche_cayenne_parse_data(instance);

        uint32_t serial = 0;
        if(flipper_format_read_uint32(flipper_format, "Serial", &serial, 1)) {
            instance->generic.serial = serial & 0xFFFFFF;
        }

        uint32_t cnt = 0;
        if(flipper_format_read_uint32(flipper_format, "Cnt", &cnt, 1)) {
            instance->generic.cnt = cnt;
        }

        uint32_t btn = 0;
        if(flipper_format_read_uint32(flipper_format, "Btn", &btn, 1)) {
            instance->generic.btn = (uint8_t)btn;
        }
    }

    return ret;
}

void subghz_protocol_decoder_porsche_cayenne_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderPorscheCayenne* instance = context;

    uint8_t frame_type = (uint8_t)((instance->generic.data >> 56) & 0x07);
    const char* frame_type_name = "??";
    if(frame_type == 0x02) {
        frame_type_name = "First";
    } else if(frame_type == 0x01) {
        frame_type_name = "Cont";
    } else if(frame_type == 0x04) {
        frame_type_name = "Final";
    }

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Sn:%06lX Btn:%X\r\n"
        "Cnt:%04lX FT:%s\r\n"
        "Raw:%08lX%08lX\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        (unsigned long)(instance->generic.serial & 0xFFFFFF),
        (unsigned int)instance->generic.btn,
        (unsigned long)instance->generic.cnt,
        frame_type_name,
        (unsigned long)(instance->generic.data >> 32),
        (unsigned long)(instance->generic.data & 0xFFFFFFFF));
}
