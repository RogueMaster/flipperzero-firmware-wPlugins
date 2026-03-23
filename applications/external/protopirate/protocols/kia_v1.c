#include "kia_v1.h"
#include "../protopirate_app_i.h"
#include <lib/toolbox/manchester_decoder.h>

#define TAG "KiaV1"

#define KIA_V1_TOTAL_BURSTS       3
#define KIA_V1_INTER_BURST_GAP_US 25000
#define KIA_V1_HEADER_PULSES      90

static const SubGhzBlockConst kia_protocol_v1_const = {
    .te_short = 800,
    .te_long = 1600,
    .te_delta = 200,
    .min_count_bit_for_found = 57,
};

struct SubGhzProtocolDecoderKiaV1 {
    SubGhzProtocolDecoderBase base;

    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    uint16_t header_count;
    ManchesterState manchester_saved_state;
    uint8_t crc;
    bool crc_check;
};

struct SubGhzProtocolEncoderKiaV1 {
    SubGhzProtocolEncoderBase base;

    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;
};

typedef enum {
    KiaV1DecoderStepReset = 0,
    KiaV1DecoderStepCheckPreamble,
    KiaV1DecoderStepDecodeData,
} KiaV1DecoderStep;

const SubGhzProtocolDecoder kia_protocol_v1_decoder = {
    .alloc = kia_protocol_decoder_v1_alloc,
    .free = kia_protocol_decoder_v1_free,

    .feed = kia_protocol_decoder_v1_feed,
    .reset = kia_protocol_decoder_v1_reset,

    .get_hash_data = kia_protocol_decoder_v1_get_hash_data,
    .serialize = kia_protocol_decoder_v1_serialize,
    .deserialize = kia_protocol_decoder_v1_deserialize,
    .get_string = kia_protocol_decoder_v1_get_string,
};

const SubGhzProtocolEncoder kia_protocol_v1_encoder = {
    .alloc = kia_protocol_encoder_v1_alloc,
    .free = kia_protocol_encoder_v1_free,

    .deserialize = kia_protocol_encoder_v1_deserialize,
    .stop = kia_protocol_encoder_v1_stop,
    .yield = kia_protocol_encoder_v1_yield,
};

const SubGhzProtocol kia_protocol_v1 = {
    .name = KIA_PROTOCOL_V1_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Send,

    .decoder = &kia_protocol_v1_decoder,
    .encoder = &kia_protocol_v1_encoder,
};

static void kia_v1_check_remote_controller(SubGhzProtocolDecoderKiaV1* instance);

static uint8_t kia_v1_crc4(const uint8_t* bytes, int count, uint8_t offset) {
    uint8_t crc = 0;

    for(int i = 0; i < count; i++) {
        uint8_t b = bytes[i];
        crc ^= ((b & 0x0F) ^ (b >> 4));
    }

    crc = (crc + offset) & 0x0F;
    return crc;
}

static void kia_v1_check_remote_controller(SubGhzProtocolDecoderKiaV1* instance) {
    instance->generic.serial = instance->generic.data >> 24;
    instance->generic.btn = (instance->generic.data >> 16) & 0xFF;
    instance->generic.cnt = ((instance->generic.data >> 4) & 0xF) << 8 |
                            ((instance->generic.data >> 8) & 0xFF);

    uint8_t cnt_high = (instance->generic.cnt >> 8) & 0xF;
    uint8_t char_data[7];
    char_data[0] = (instance->generic.serial >> 24) & 0xFF;
    char_data[1] = (instance->generic.serial >> 16) & 0xFF;
    char_data[2] = (instance->generic.serial >> 8) & 0xFF;
    char_data[3] = instance->generic.serial & 0xFF;
    char_data[4] = instance->generic.btn;
    char_data[5] = instance->generic.cnt & 0xFF;

    char_data[6] = cnt_high;
    uint8_t crc = kia_v1_crc4(char_data, 7, 1);

    instance->crc = cnt_high << 4 | crc;
    instance->crc_check = (crc == (instance->generic.data & 0xF));
}

static const char* kia_v1_get_button_name(uint8_t btn) {
    const char* name;
    switch(btn) {
    case 0x1:
        name = "Close";
        break;
    case 0x2:
        name = "Open";
        break;
    case 0x3:
        name = "Boot";
        break;
    default:
        name = "??";
        break;
    }
    return name;
}
void* kia_protocol_encoder_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderKiaV1* instance = malloc(sizeof(SubGhzProtocolEncoderKiaV1));

    instance->base.protocol = &kia_protocol_v1;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 1200;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    return instance;
}

void kia_protocol_encoder_v1_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    free(instance->encoder.upload);
    free(instance);
}

static void kia_protocol_encoder_v1_get_upload(SubGhzProtocolEncoderKiaV1* instance) {
    furi_check(instance);
    size_t index = 0;

    uint8_t cnt_high = (instance->generic.cnt >> 8) & 0xF;
    uint8_t char_data[7];
    char_data[0] = (instance->generic.serial >> 24) & 0xFF;
    char_data[1] = (instance->generic.serial >> 16) & 0xFF;
    char_data[2] = (instance->generic.serial >> 8) & 0xFF;
    char_data[3] = instance->generic.serial & 0xFF;
    char_data[4] = instance->generic.btn;
    char_data[5] = instance->generic.cnt & 0xFF;

    char_data[6] = cnt_high;
    uint8_t crc = kia_v1_crc4(char_data, 7, 1);

    instance->generic.data = (uint64_t)instance->generic.serial << 24 |
                             instance->generic.btn << 16 | (instance->generic.cnt & 0xFF) << 8 |
                             ((instance->generic.cnt >> 8) & 0xF) << 4 | crc;

    for(uint8_t burst = 0; burst < KIA_V1_TOTAL_BURSTS; burst++) {
        if(burst > 0) {
            instance->encoder.upload[index++] =
                level_duration_make(false, KIA_V1_INTER_BURST_GAP_US);
        }

        for(int i = 0; i < KIA_V1_HEADER_PULSES; i++) {
            instance->encoder.upload[index++] =
                level_duration_make(false, (uint32_t)kia_protocol_v1_const.te_long);
            instance->encoder.upload[index++] =
                level_duration_make(true, (uint32_t)kia_protocol_v1_const.te_long);
        }

        instance->encoder.upload[index++] =
            level_duration_make(false, (uint32_t)kia_protocol_v1_const.te_short);

        for(uint8_t i = instance->generic.data_count_bit; i > 1; i--) {
            if(bit_read(instance->generic.data, i - 2)) {
                instance->encoder.upload[index++] =
                    level_duration_make(true, (uint32_t)kia_protocol_v1_const.te_short);
                instance->encoder.upload[index++] =
                    level_duration_make(false, (uint32_t)kia_protocol_v1_const.te_short);
            } else {
                instance->encoder.upload[index++] =
                    level_duration_make(false, (uint32_t)kia_protocol_v1_const.te_short);
                instance->encoder.upload[index++] =
                    level_duration_make(true, (uint32_t)kia_protocol_v1_const.te_short);
            }
        }
    }

    instance->encoder.size_upload = index;
    instance->encoder.front = 0;

    FURI_LOG_I(
        TAG,
        "Upload built: %d bursts, size_upload=%zu, data_count_bit=%u, data=0x%016llX",
        KIA_V1_TOTAL_BURSTS,
        instance->encoder.size_upload,
        instance->generic.data_count_bit,
        instance->generic.data);
}

SubGhzProtocolStatus
    kia_protocol_encoder_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    flipper_format_rewind(flipper_format);

    do {
        FuriString* temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            furi_string_free(temp_str);
            break;
        }

        if(!furi_string_equal(temp_str, instance->base.protocol->name)) {
            FURI_LOG_E(
                TAG,
                "Wrong protocol %s != %s",
                furi_string_get_cstr(temp_str),
                instance->base.protocol->name);
            furi_string_free(temp_str);
            break;
        }
        furi_string_free(temp_str);

        uint32_t bit_count_temp;
        if(!flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1)) {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }

        instance->generic.data_count_bit = kia_protocol_v1_const.min_count_bit_for_found;

        temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            FURI_LOG_E(TAG, "Missing Key");
            furi_string_free(temp_str);
            break;
        }

        const char* key_str = furi_string_get_cstr(temp_str);
        uint64_t key = 0;
        size_t str_len = strlen(key_str);
        size_t hex_pos = 0;
        for(size_t i = 0; i < str_len && hex_pos < 16; i++) {
            char c = key_str[i];
            if(c == ' ') continue;

            uint8_t nibble;
            if(c >= '0' && c <= '9') {
                nibble = c - '0';
            } else if(c >= 'A' && c <= 'F') {
                nibble = c - 'A' + 10;
            } else if(c >= 'a' && c <= 'f') {
                nibble = c - 'a' + 10;
            } else {
                FURI_LOG_E(TAG, "Invalid hex character: %c", c);
                furi_string_free(temp_str);
                break;
            }

            key = (key << 4) | nibble;
            hex_pos++;
        }

        furi_string_free(temp_str);

        if(hex_pos != 16) {
            FURI_LOG_E(TAG, "Invalid key length: %zu nibbles (expected 16)", hex_pos);
            break;
        }

        instance->generic.data = key;
        FURI_LOG_I(TAG, "Parsed key: 0x%016llX", instance->generic.data);

        if(instance->generic.data == 0) {
            FURI_LOG_E(TAG, "Key is zero after parsing!");
            break;
        }

        if(!flipper_format_read_uint32(flipper_format, "Serial", &instance->generic.serial, 1)) {
            instance->generic.serial = instance->generic.data >> 24;
            FURI_LOG_I(TAG, "Extracted serial: 0x%08lX", instance->generic.serial);
        } else {
            FURI_LOG_I(TAG, "Read serial: 0x%08lX", instance->generic.serial);
        }

        uint32_t btn_temp;
        if(flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1)) {
            instance->generic.btn = (uint8_t)btn_temp;
            FURI_LOG_I(TAG, "Read button: 0x%02X", instance->generic.btn);
        } else {
            instance->generic.btn = (instance->generic.data >> 16) & 0xFF;
            FURI_LOG_I(TAG, "Extracted button: 0x%02X", instance->generic.btn);
        }

        uint32_t cnt_temp;
        if(flipper_format_read_uint32(flipper_format, "Cnt", &cnt_temp, 1)) {
            instance->generic.cnt = (uint16_t)cnt_temp;
            FURI_LOG_I(TAG, "Read counter: 0x%03lX", (unsigned long)instance->generic.cnt);
        } else {
            instance->generic.cnt = ((instance->generic.data >> 4) & 0xF) << 8 |
                                    ((instance->generic.data >> 8) & 0xFF);
            FURI_LOG_I(TAG, "Extracted counter: 0x%03lX", (unsigned long)instance->generic.cnt);
        }

        if(!flipper_format_read_uint32(
               flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1)) {
            instance->encoder.repeat = 10;
            FURI_LOG_D(
                TAG, "Repeat not found in file, using default 10 for continuous transmission");
        }

        kia_protocol_encoder_v1_get_upload(instance);

        instance->encoder.is_running = true;

        FURI_LOG_I(
            TAG,
            "Encoder deserialized: repeat=%u, size_upload=%zu, is_running=%d, front=%zu",
            instance->encoder.repeat,
            instance->encoder.size_upload,
            instance->encoder.is_running,
            instance->encoder.front);

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void kia_protocol_encoder_v1_stop(void* context) {
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration kia_protocol_encoder_v1_yield(void* context) {
    SubGhzProtocolEncoderKiaV1* instance = context;

    if(instance->encoder.repeat == 0 || !instance->encoder.is_running) {
        FURI_LOG_D(
            TAG,
            "Encoder yield stopped: repeat=%u, is_running=%d",
            instance->encoder.repeat,
            instance->encoder.is_running);
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(instance->encoder.front < 5 || instance->encoder.front == 0) {
        FURI_LOG_D(
            TAG,
            "Encoder yield[%zu]: repeat=%u, size=%zu, level=%d, duration=%lu",
            instance->encoder.front,
            instance->encoder.repeat,
            instance->encoder.size_upload,
            level_duration_get_level(ret),
            level_duration_get_duration(ret));
    }

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
        FURI_LOG_I(
            TAG, "Encoder completed one cycle, remaining repeat=%u", instance->encoder.repeat);
    }

    return ret;
}

void kia_protocol_encoder_v1_set_button(void* context, uint8_t button) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->generic.btn = button & 0xFF;
    kia_protocol_encoder_v1_get_upload(instance);
    FURI_LOG_I(TAG, "Button set to 0x%02X, upload rebuilt with new CRC", instance->generic.btn);
}

void kia_protocol_encoder_v1_set_counter(void* context, uint16_t counter) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->generic.cnt = counter & 0xFFF;
    kia_protocol_encoder_v1_get_upload(instance);
    FURI_LOG_I(
        TAG,
        "Counter set to 0x%03X, upload rebuilt with new CRC",
        (uint16_t)instance->generic.cnt);
}

void kia_protocol_encoder_v1_increment_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    instance->generic.cnt = (instance->generic.cnt + 1) & 0xFFF;
    kia_protocol_encoder_v1_get_upload(instance);
    FURI_LOG_I(
        TAG,
        "Counter incremented to 0x%03X, upload rebuilt with new CRC",
        (uint16_t)instance->generic.cnt);
}

uint16_t kia_protocol_encoder_v1_get_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    return instance->generic.cnt;
}

uint8_t kia_protocol_encoder_v1_get_button(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV1* instance = context;
    return instance->generic.btn;
}

void* kia_protocol_decoder_v1_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV1* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV1));
    instance->base.protocol = &kia_protocol_v1;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void kia_protocol_decoder_v1_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    free(instance);
}

void kia_protocol_decoder_v1_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    instance->decoder.parser_step = KiaV1DecoderStepReset;
}

void kia_protocol_decoder_v1_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    ManchesterEvent event = ManchesterEventReset;

    switch(instance->decoder.parser_step) {
    case KiaV1DecoderStepReset:
        if((level) && (DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                       kia_protocol_v1_const.te_delta)) {
            instance->decoder.parser_step = KiaV1DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            manchester_advance(
                instance->manchester_saved_state,
                ManchesterEventReset,
                &instance->manchester_saved_state,
                NULL);
        }
        break;

    case KiaV1DecoderStepCheckPreamble:
        if(!level) {
            if((DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta) &&
               (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta)) {
                instance->header_count++;
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KiaV1DecoderStepReset;
            }
        }
        if(instance->header_count > 70) {
            if((!level) &&
               (DURATION_DIFF(duration, kia_protocol_v1_const.te_short) <
                kia_protocol_v1_const.te_delta) &&
               (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v1_const.te_long) <
                kia_protocol_v1_const.te_delta)) {
                instance->decoder.decode_count_bit = 1;
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->header_count = 0;
                instance->decoder.parser_step = KiaV1DecoderStepDecodeData;
            }
        }
        break;

    case KiaV1DecoderStepDecodeData:
        if((DURATION_DIFF(duration, kia_protocol_v1_const.te_short) <
            kia_protocol_v1_const.te_delta)) {
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        } else if((DURATION_DIFF(duration, kia_protocol_v1_const.te_long) <
                   kia_protocol_v1_const.te_delta)) {
            event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
        }

        if(event != ManchesterEventReset) {
            bool data;
            bool data_ok = manchester_advance(
                instance->manchester_saved_state, event, &instance->manchester_saved_state, &data);
            if(data_ok) {
                instance->decoder.decode_data = (instance->decoder.decode_data << 1) | data;
                instance->decoder.decode_count_bit++;
            }
        }

        if(instance->decoder.decode_count_bit == kia_protocol_v1_const.min_count_bit_for_found) {
            instance->generic.data = instance->decoder.decode_data;
            instance->generic.data_count_bit = instance->decoder.decode_count_bit;
            if(instance->base.callback)
                instance->base.callback(&instance->base, instance->base.context);

            instance->decoder.decode_data = 0;
            instance->decoder.decode_count_bit = 0;
            instance->decoder.parser_step = KiaV1DecoderStepReset;
        }
        break;
    }
}

uint8_t kia_protocol_decoder_v1_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v1_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    kia_v1_check_remote_controller(instance);

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Preset", furi_string_get_cstr(preset->name)))
            break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Protocol", instance->generic.protocol_name))
            break;

        uint32_t bits = instance->generic.data_count_bit;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &bits, 1)) break;

        char key_str[20];
        snprintf(key_str, sizeof(key_str), "%016llX", instance->generic.data);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key_str)) break;

        if(!flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1))
            break;

        uint32_t temp = instance->generic.btn;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp, 1)) break;

        temp = instance->generic.cnt;
        if(!flipper_format_write_uint32(flipper_format, "Cnt", &temp, 1)) break;

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    kia_protocol_decoder_v1_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;
    flipper_format_rewind(flipper_format);
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, kia_protocol_v1_const.min_count_bit_for_found);
}

void kia_protocol_decoder_v1_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV1* instance = context;

    kia_v1_check_remote_controller(instance);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0xFFFFFFFF;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%06lX%08lX\r\n"
        "Serial:%08lX\r\n"
        "Cnt:%03lX CRC:%01X %s\r\n"
        "Btn:%02X:%s\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.cnt,
        instance->crc,
        instance->crc_check ? "OK" : "WRONG",
        instance->generic.btn,
        kia_v1_get_button_name(instance->generic.btn));
}
