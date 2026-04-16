#include "kia_v7.h"
#include <string.h>

#define KIA_V7_UPLOAD_CAPACITY    0x3A4
#define KIA_V7_PREAMBLE_PAIRS     0x13F
#define KIA_V7_PREAMBLE_MIN_PAIRS 16
#define KIA_V7_HEADER             0x4C
#define KIA_V7_TAIL_GAP_US        0x7D0
#define KIA_V7_KEY_BITS           64U
#define KIA_V7_DEFAULT_TX_REPEAT  10U

static const SubGhzBlockConst kia_protocol_v7_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = KIA_V7_KEY_BITS,
};

typedef enum {
    KiaV7DecoderStepReset = 0,
    KiaV7DecoderStepPreamble = 1,
    KiaV7DecoderStepSyncLow = 2,
    KiaV7DecoderStepData = 3,
} KiaV7DecoderStep;

struct SubGhzProtocolDecoderKiaV7 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    ManchesterState manchester_state;
    uint16_t preamble_count;

    uint8_t decoded_button;
    uint8_t fixed_high_byte;
    uint8_t crc_calculated;
    uint8_t crc_raw;
    bool crc_valid;
};

#ifdef ENABLE_EMULATE_FEATURE
struct SubGhzProtocolEncoderKiaV7 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint8_t tx_bit_count;
    uint8_t decoded_button;
    uint8_t fixed_high_byte;
    uint8_t crc_calculated;
    uint8_t crc_raw;
    bool crc_valid;
};
#endif

static uint8_t kia_v7_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x4CU;

    for(size_t index = 0; index < len; index++) {
        crc ^= data[index];
        for(uint8_t bit = 0; bit < 8; bit++) {
            const bool msb = (crc & 0x80U) != 0U;
            crc <<= 1U;
            if(msb) {
                crc ^= 0x7FU;
            }
        }
    }

    return crc;
}

static void kia_v7_u64_to_bytes_be(uint64_t data, uint8_t bytes[8]) {
    for(size_t index = 0; index < 8; index++) {
        bytes[index] = (data >> ((7U - index) * 8U)) & 0xFFU;
    }
}

static uint64_t kia_v7_bytes_to_u64_be(const uint8_t bytes[8]) {
    uint64_t data = 0;

    for(size_t index = 0; index < 8; index++) {
        data = (data << 8U) | bytes[index];
    }

    return data;
}

static bool kia_v7_is_short(uint32_t duration) {
    return DURATION_DIFF(duration, kia_protocol_v7_const.te_short) <
           kia_protocol_v7_const.te_delta;
}

static bool kia_v7_is_long(uint32_t duration) {
    return DURATION_DIFF(duration, kia_protocol_v7_const.te_long) < kia_protocol_v7_const.te_delta;
}

static const char* kia_v7_get_button_name(uint8_t button) {
    switch(button) {
    case 0x01:
        return "Lock";
    case 0x02:
        return "Unlock";
    case 0x03:
    case 0x08:
        return "Trunk";
    default:
        return "??";
    }
}

static SubGhzProtocolStatus
    kia_v7_write_display(FlipperFormat* flipper_format, const char* protocol_name, uint8_t button) {
    SubGhzProtocolStatus status = SubGhzProtocolStatusOk;
    FuriString* display = furi_string_alloc();

    furi_string_printf(display, "%s - %s", protocol_name, kia_v7_get_button_name(button));

    if(!flipper_format_write_string_cstr(flipper_format, "Disp", furi_string_get_cstr(display))) {
        status = SubGhzProtocolStatusErrorParserOthers;
    }

    furi_string_free(display);
    return status;
}

static void kia_v7_decode_key_common(
    SubGhzBlockGeneric* generic,
    uint8_t* decoded_button,
    uint8_t* fixed_high_byte,
    uint8_t* crc_calculated,
    uint8_t* crc_raw,
    bool* crc_valid) {
    uint8_t bytes[8];
    kia_v7_u64_to_bytes_be(generic->data, bytes);

    const uint32_t serial = (((uint32_t)bytes[3]) << 20U) | (((uint32_t)bytes[4]) << 12U) |
                            (((uint32_t)bytes[5]) << 4U) | (((uint32_t)bytes[6]) >> 4U);
    const uint16_t counter = ((uint16_t)bytes[1] << 8U) | (uint16_t)bytes[2];
    const uint8_t button = bytes[6] & 0x0FU;
    const uint8_t crc_calc = kia_v7_crc8(bytes, 7);
    const uint8_t crc_pkt = bytes[7];

    generic->serial = serial & 0x0FFFFFFFU;
    generic->btn = button;
    generic->cnt = counter;
    generic->data_count_bit = KIA_V7_KEY_BITS;

    if(decoded_button) {
        *decoded_button = button;
    }
    if(fixed_high_byte) {
        *fixed_high_byte = bytes[0];
    }
    if(crc_calculated) {
        *crc_calculated = crc_calc;
    }
    if(crc_raw) {
        *crc_raw = crc_pkt;
    }
    if(crc_valid) {
        *crc_valid = (crc_calc == crc_pkt);
    }
}

static void kia_v7_decode_key_decoder(SubGhzProtocolDecoderKiaV7* instance) {
    kia_v7_decode_key_common(
        &instance->generic,
        &instance->decoded_button,
        &instance->fixed_high_byte,
        &instance->crc_calculated,
        &instance->crc_raw,
        &instance->crc_valid);
}

static uint64_t kia_v7_encode_key(
    uint8_t fixed_high_byte,
    uint32_t serial,
    uint8_t button,
    uint16_t counter,
    uint8_t* crc_out) {
    uint8_t bytes[8];

    serial &= 0x0FFFFFFFU;
    button &= 0x0FU;

    bytes[0] = fixed_high_byte;
    bytes[1] = (counter >> 8U) & 0xFFU;
    bytes[2] = counter & 0xFFU;
    bytes[3] = (serial >> 20U) & 0xFFU;
    bytes[4] = (serial >> 12U) & 0xFFU;
    bytes[5] = (serial >> 4U) & 0xFFU;
    bytes[6] = ((serial & 0x0FU) << 4U) | button;
    bytes[7] = kia_v7_crc8(bytes, 7);

    if(crc_out) {
        *crc_out = bytes[7];
    }

    return kia_v7_bytes_to_u64_be(bytes);
}

#ifdef ENABLE_EMULATE_FEATURE
static void kia_v7_decode_key_encoder(SubGhzProtocolEncoderKiaV7* instance) {
    kia_v7_decode_key_common(
        &instance->generic,
        &instance->decoded_button,
        &instance->fixed_high_byte,
        &instance->crc_calculated,
        &instance->crc_raw,
        &instance->crc_valid);
}

static bool kia_v7_encoder_get_upload(SubGhzProtocolEncoderKiaV7* instance) {
    furi_check(instance);

    const LevelDuration high_short = level_duration_make(true, kia_protocol_v7_const.te_short);
    const LevelDuration low_short = level_duration_make(false, kia_protocol_v7_const.te_short);
    const LevelDuration low_tail = level_duration_make(false, KIA_V7_TAIL_GAP_US);
    const size_t max_size = KIA_V7_UPLOAD_CAPACITY;

    const uint8_t bit_count = (instance->tx_bit_count > 0U && instance->tx_bit_count <= 64U) ?
                                  instance->tx_bit_count :
                                  64U;

    size_t final_size = 0;

    for(uint8_t pass = 0; pass < 2; pass++) {
        size_t index = pass;

        for(size_t i = 0; i < KIA_V7_PREAMBLE_PAIRS; i++) {
            if((index + 2U) > max_size) {
                return false;
            }

            instance->encoder.upload[index++] = high_short;
            instance->encoder.upload[index++] = low_short;
        }

        if((index + 1U) > max_size) {
            return false;
        }
        instance->encoder.upload[index++] = high_short;

        for(int32_t bit = (int32_t)bit_count - 1; bit >= 0; bit--) {
            if((index + 2U) > max_size) {
                return false;
            }

            const bool value = ((instance->generic.data >> bit) & 1ULL) != 0ULL;
            instance->encoder.upload[index++] = value ? high_short : low_short;
            instance->encoder.upload[index++] = value ? low_short : high_short;
        }

        if((index + 2U) > max_size) {
            return false;
        }
        instance->encoder.upload[index++] = high_short;
        instance->encoder.upload[index++] = low_tail;

        final_size = index;
    }

    instance->encoder.front = 0;
    instance->encoder.size_upload = final_size;
    return true;
}
#endif

const SubGhzProtocolDecoder kia_protocol_v7_decoder = {
    .alloc = kia_protocol_decoder_v7_alloc,
    .free = kia_protocol_decoder_v7_free,
    .feed = kia_protocol_decoder_v7_feed,
    .reset = kia_protocol_decoder_v7_reset,
    .get_hash_data = kia_protocol_decoder_v7_get_hash_data,
    .serialize = kia_protocol_decoder_v7_serialize,
    .deserialize = kia_protocol_decoder_v7_deserialize,
    .get_string = kia_protocol_decoder_v7_get_string,
};

#ifdef ENABLE_EMULATE_FEATURE
const SubGhzProtocolEncoder kia_protocol_v7_encoder = {
    .alloc = kia_protocol_encoder_v7_alloc,
    .free = kia_protocol_encoder_v7_free,
    .deserialize = kia_protocol_encoder_v7_deserialize,
    .stop = kia_protocol_encoder_v7_stop,
    .yield = kia_protocol_encoder_v7_yield,
};
#else
const SubGhzProtocolEncoder kia_protocol_v7_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};
#endif

const SubGhzProtocol kia_protocol_v7 = {
    .name = KIA_PROTOCOL_V7_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &kia_protocol_v7_decoder,
    .encoder = &kia_protocol_v7_encoder,
};

#ifdef ENABLE_EMULATE_FEATURE
void* kia_protocol_encoder_v7_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolEncoderKiaV7* instance = calloc(1, sizeof(SubGhzProtocolEncoderKiaV7));
    furi_check(instance);

    instance->base.protocol = &kia_protocol_v7;
    instance->generic.protocol_name = instance->base.protocol->name;
    instance->encoder.repeat = 1;
    instance->encoder.size_upload = KIA_V7_UPLOAD_CAPACITY;
    instance->encoder.upload = malloc(KIA_V7_UPLOAD_CAPACITY * sizeof(LevelDuration));
    furi_check(instance->encoder.upload);

    return instance;
}

void kia_protocol_encoder_v7_free(void* context) {
    furi_check(context);

    SubGhzProtocolEncoderKiaV7* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

SubGhzProtocolStatus
    kia_protocol_encoder_v7_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);

    SubGhzProtocolEncoderKiaV7* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    instance->encoder.repeat = KIA_V7_DEFAULT_TX_REPEAT;

    do {
        FuriString* temp_str = furi_string_alloc();
        if(!temp_str) {
            break;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            furi_string_free(temp_str);
            break;
        }

        if(!furi_string_equal(temp_str, instance->base.protocol->name)) {
            furi_string_free(temp_str);
            break;
        }
        furi_string_free(temp_str);

        flipper_format_rewind(flipper_format);
        SubGhzProtocolStatus load_st = subghz_block_generic_deserialize_check_count_bit(
            &instance->generic, flipper_format, KIA_V7_KEY_BITS);
        if(load_st != SubGhzProtocolStatusOk) {
            break;
        }

        instance->tx_bit_count =
            (instance->generic.data_count_bit > 0U && instance->generic.data_count_bit <= 64U) ?
                (uint8_t)instance->generic.data_count_bit :
                64U;

        kia_v7_decode_key_encoder(instance);

        uint32_t u32 = 0;
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Serial", &u32, 1)) {
            instance->generic.serial = u32;
        }
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Btn", &u32, 1)) {
            instance->generic.btn = (uint8_t)u32;
        }
        flipper_format_rewind(flipper_format);
        if(flipper_format_read_uint32(flipper_format, "Cnt", &u32, 1)) {
            instance->generic.cnt = (uint16_t)u32;
        }

        instance->generic.btn &= 0x0FU;
        instance->generic.cnt &= 0xFFFFU;
        instance->generic.serial &= 0x0FFFFFFFU;

        instance->generic.data = kia_v7_encode_key(
            instance->fixed_high_byte,
            instance->generic.serial,
            instance->generic.btn,
            (uint16_t)instance->generic.cnt,
            &instance->crc_calculated);
        instance->generic.data_count_bit = KIA_V7_KEY_BITS;

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Repeat", &u32, 1)) {
            u32 = KIA_V7_DEFAULT_TX_REPEAT;
        }
        instance->encoder.repeat = u32;

        if(!kia_v7_encoder_get_upload(instance)) {
            break;
        }

        if(instance->encoder.size_upload == 0) {
            break;
        }

        flipper_format_rewind(flipper_format);
        uint8_t key_data[sizeof(uint64_t)];
        kia_v7_u64_to_bytes_be(instance->generic.data, key_data);
        if(!flipper_format_update_hex(flipper_format, "Key", key_data, sizeof(key_data))) {
            break;
        }

        instance->encoder.is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void kia_protocol_encoder_v7_stop(void* context) {
    SubGhzProtocolEncoderKiaV7* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration kia_protocol_encoder_v7_yield(void* context) {
    SubGhzProtocolEncoderKiaV7* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration duration = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return duration;
}
#endif

void* kia_protocol_decoder_v7_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);

    SubGhzProtocolDecoderKiaV7* instance = calloc(1, sizeof(SubGhzProtocolDecoderKiaV7));
    furi_check(instance);

    instance->base.protocol = &kia_protocol_v7;
    instance->generic.protocol_name = instance->base.protocol->name;

    return instance;
}

void kia_protocol_decoder_v7_free(void* context) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    free(instance);
}

void kia_protocol_decoder_v7_reset(void* context) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    instance->decoder.parser_step = KiaV7DecoderStepReset;
    instance->decoder.te_last = 0;
    instance->decoder.decode_data = 0;
    instance->decoder.decode_count_bit = 0;
    instance->preamble_count = 0;
    manchester_advance(
        instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
}

void kia_protocol_decoder_v7_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    ManchesterEvent event = ManchesterEventReset;
    bool data = false;

    switch(instance->decoder.parser_step) {
    case KiaV7DecoderStepReset:
        if(level && kia_v7_is_short(duration)) {
            instance->decoder.parser_step = KiaV7DecoderStepPreamble;
            instance->decoder.te_last = duration;
            instance->preamble_count = 0;
            manchester_advance(
                instance->manchester_state,
                ManchesterEventReset,
                &instance->manchester_state,
                NULL);
        }
        break;

    case KiaV7DecoderStepPreamble:
        if(level) {
            if(kia_v7_is_long(duration) && kia_v7_is_short(instance->decoder.te_last)) {
                if(instance->preamble_count > (KIA_V7_PREAMBLE_MIN_PAIRS - 1U)) {
                    instance->decoder.decode_data = 0;
                    instance->decoder.decode_count_bit = 0;
                    instance->preamble_count = 0;

                    subghz_protocol_blocks_add_bit(&instance->decoder, 1U);
                    subghz_protocol_blocks_add_bit(&instance->decoder, 0U);
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1U);
                    subghz_protocol_blocks_add_bit(&instance->decoder, 1U);

                    instance->decoder.te_last = duration;
                    instance->decoder.parser_step = KiaV7DecoderStepSyncLow;
                } else {
                    instance->decoder.parser_step = KiaV7DecoderStepReset;
                }
            } else if(kia_v7_is_short(duration)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KiaV7DecoderStepReset;
            }
        } else {
            if(kia_v7_is_short(duration) && kia_v7_is_short(instance->decoder.te_last)) {
                instance->preamble_count++;
            } else {
                instance->decoder.parser_step = KiaV7DecoderStepReset;
            }
        }
        break;

    case KiaV7DecoderStepSyncLow:
        if(!level && kia_v7_is_short(duration) && kia_v7_is_long(instance->decoder.te_last)) {
            instance->decoder.te_last = duration;
            instance->decoder.parser_step = KiaV7DecoderStepData;
        }
        break;

    case KiaV7DecoderStepData: {
        if(kia_v7_is_short(duration)) {
            event = (ManchesterEvent)((uint8_t)(level & 1U) << 1U);
        } else if(kia_v7_is_long(duration)) {
            event = level ? ManchesterEventLongHigh : ManchesterEventLongLow;
        } else {
            event = ManchesterEventReset;
        }

        if(kia_v7_is_short(duration) || kia_v7_is_long(duration)) {
            if(manchester_advance(
                   instance->manchester_state, event, &instance->manchester_state, &data)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, data);
            }
        }

        if(instance->decoder.decode_count_bit == KIA_V7_KEY_BITS) {
            const uint64_t candidate = ~instance->decoder.decode_data;
            const uint8_t hdr = (uint8_t)((candidate >> 56U) & 0xFFU);

            if(hdr == KIA_V7_HEADER) {
                instance->generic.data = candidate;
                instance->generic.data_count_bit = KIA_V7_KEY_BITS;
                kia_v7_decode_key_decoder(instance);

                if(instance->crc_valid) {
                    if(instance->base.callback) {
                        instance->base.callback(&instance->base, instance->base.context);
                    }
                } else {
                    instance->generic.data = 0;
                    instance->generic.data_count_bit = 0;
                }

                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = KiaV7DecoderStepReset;
                manchester_advance(
                    instance->manchester_state,
                    ManchesterEventReset,
                    &instance->manchester_state,
                    NULL);
            } else {
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                instance->decoder.parser_step = KiaV7DecoderStepReset;
                manchester_advance(
                    instance->manchester_state,
                    ManchesterEventReset,
                    &instance->manchester_state,
                    NULL);
            }
        }

        break;
    }
    }
}

uint8_t kia_protocol_decoder_v7_get_hash_data(void* context) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit >> 3U) + 1U);
}

void kia_protocol_decoder_v7_get_string(void* context, FuriString* output) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    kia_v7_decode_key_decoder(instance);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%016llX\r\n"
        "Sn:%07lX Cnt:%04lX\r\n"
        "Btn:%01X [%s] CRC:%02X [%s]",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->generic.data,
        instance->generic.serial & 0x0FFFFFFFU,
        instance->generic.cnt & 0xFFFFU,
        instance->decoded_button & 0x0FU,
        kia_v7_get_button_name(instance->decoded_button),
        instance->crc_calculated,
        instance->crc_valid ? "OK" : "ERR");
}

SubGhzProtocolStatus kia_protocol_decoder_v7_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    kia_v7_decode_key_decoder(instance);

    SubGhzProtocolStatus status =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    uint32_t serial = instance->generic.serial & 0x0FFFFFFFU;
    if(!flipper_format_write_uint32(flipper_format, "Serial", &serial, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    uint32_t btn_u32 = (uint32_t)(instance->decoded_button & 0x0FU);
    if(!flipper_format_write_uint32(flipper_format, "Btn", &btn_u32, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    uint32_t cnt_u32 = (uint32_t)(instance->generic.cnt & 0xFFFFU);
    if(!flipper_format_write_uint32(flipper_format, "Cnt", &cnt_u32, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }
    uint32_t repeat_u32 = KIA_V7_DEFAULT_TX_REPEAT;
    if(!flipper_format_write_uint32(flipper_format, "Repeat", &repeat_u32, 1)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    return kia_v7_write_display(
        flipper_format, instance->generic.protocol_name, instance->decoded_button);
}

SubGhzProtocolStatus
    kia_protocol_decoder_v7_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);

    SubGhzProtocolDecoderKiaV7* instance = context;
    SubGhzProtocolStatus status = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, KIA_V7_KEY_BITS);

    if(status != SubGhzProtocolStatusOk) {
        return status;
    }

    if(!flipper_format_rewind(flipper_format)) {
        return SubGhzProtocolStatusErrorParserOthers;
    }

    kia_v7_decode_key_decoder(instance);

    uint32_t ser_u32 = 0;
    uint32_t btn_u32 = 0;
    uint32_t cnt_u32 = 0;
    bool got_serial = false;
    bool got_btn = false;
    bool got_cnt = false;

    flipper_format_rewind(flipper_format);
    got_serial = flipper_format_read_uint32(flipper_format, "Serial", &ser_u32, 1);
    flipper_format_rewind(flipper_format);
    got_btn = flipper_format_read_uint32(flipper_format, "Btn", &btn_u32, 1);
    flipper_format_rewind(flipper_format);
    got_cnt = flipper_format_read_uint32(flipper_format, "Cnt", &cnt_u32, 1);

    if(got_serial || got_btn || got_cnt) {
        if(got_serial) {
            instance->generic.serial = ser_u32 & 0x0FFFFFFFU;
        }
        if(got_btn) {
            instance->generic.btn = (uint8_t)(btn_u32 & 0x0FU);
        }
        if(got_cnt) {
            instance->generic.cnt = (uint16_t)(cnt_u32 & 0xFFFFU);
        }
        instance->generic.data = kia_v7_encode_key(
            instance->fixed_high_byte,
            instance->generic.serial,
            instance->generic.btn & 0x0FU,
            (uint16_t)(instance->generic.cnt & 0xFFFFU),
            &instance->crc_calculated);
        kia_v7_decode_key_decoder(instance);
    }

    return SubGhzProtocolStatusOk;
}
