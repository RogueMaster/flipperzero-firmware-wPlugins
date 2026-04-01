#include "kia_v0.h"
#include "../protopirate_app_i.h"

#define TAG "KiaProtocolV0"

static const SubGhzBlockConst subghz_protocol_kia_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 61,
};

// Multi-burst configuration
#define KIA_TOTAL_BURSTS       2
#define KIA_INTER_BURST_GAP_US 25000
#define KIA_HEADER_PULSES      64

struct SubGhzProtocolDecoderKIA {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;
};

struct SubGhzProtocolEncoderKIA {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t serial;
    uint8_t button;
    uint16_t counter;
};

typedef enum {
    KIADecoderStepReset = 0,
    KIADecoderStepCheckPreambula,
    KIADecoderStepSaveDuration,
    KIADecoderStepCheckDuration,
} KIADecoderStep;

// Forward declarations for encoder
void* subghz_protocol_encoder_kia_alloc(SubGhzEnvironment* environment);
void subghz_protocol_encoder_kia_free(void* context);
SubGhzProtocolStatus
    subghz_protocol_encoder_kia_deserialize(void* context, FlipperFormat* flipper_format);
void subghz_protocol_encoder_kia_stop(void* context);
LevelDuration subghz_protocol_encoder_kia_yield(void* context);

const SubGhzProtocolDecoder subghz_protocol_kia_decoder = {
    .alloc = subghz_protocol_decoder_kia_alloc,
    .free = subghz_protocol_decoder_kia_free,
    .feed = subghz_protocol_decoder_kia_feed,
    .reset = subghz_protocol_decoder_kia_reset,
    .get_hash_data = subghz_protocol_decoder_kia_get_hash_data,
    .serialize = subghz_protocol_decoder_kia_serialize,
    .deserialize = subghz_protocol_decoder_kia_deserialize,
    .get_string = subghz_protocol_decoder_kia_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_kia_encoder = {
    .alloc = subghz_protocol_encoder_kia_alloc,
    .free = subghz_protocol_encoder_kia_free,
    .deserialize = subghz_protocol_encoder_kia_deserialize,
    .stop = subghz_protocol_encoder_kia_stop,
    .yield = subghz_protocol_encoder_kia_yield,
};

const SubGhzProtocol kia_protocol_v0 = {
    .name = KIA_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_kia_decoder,
    .encoder = &subghz_protocol_kia_encoder,
};

/**
 * CRC8 calculation for Kia protocol
 * Polynomial: 0x7F
 * Initial value: 0x00
 * MSB-first processing
 */
static uint8_t kia_crc8(uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t j = 0; j < 8; j++) {
            if((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x7F);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * Calculate CRC for the Kia data packet
 * CRC is calculated over bits 8-55 (6 bytes)
 */
static uint8_t kia_calculate_crc(uint64_t data) {
    uint8_t crc_data[6];
    crc_data[0] = (data >> 48) & 0xFF;
    crc_data[1] = (data >> 40) & 0xFF;
    crc_data[2] = (data >> 32) & 0xFF;
    crc_data[3] = (data >> 24) & 0xFF;
    crc_data[4] = (data >> 16) & 0xFF;
    crc_data[5] = (data >> 8) & 0xFF;

    return kia_crc8(crc_data, 6);
}

/**
 * Verify CRC of received data
 */
static bool kia_verify_crc(uint64_t data) {
    uint8_t received_crc = data & 0xFF;
    uint8_t calculated_crc = kia_calculate_crc(data);

    FURI_LOG_D(
        TAG,
        "CRC Check - Received: 0x%02X, Calculated: 0x%02X, Match: %s",
        received_crc,
        calculated_crc,
        (received_crc == calculated_crc) ? "YES" : "NO");

    return (received_crc == calculated_crc);
}

// ============================================================================
// ENCODER IMPLEMENTATION
// ============================================================================

void* subghz_protocol_encoder_kia_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderKIA* instance = malloc(sizeof(SubGhzProtocolEncoderKIA));
    instance->base.protocol = &kia_protocol_v0;
    instance->serial = 0;
    instance->button = 0;
    instance->counter = 0;

    instance->encoder.size_upload =
        (KIA_HEADER_PULSES + 2 + 118 + 1) * KIA_TOTAL_BURSTS + (KIA_TOTAL_BURSTS - 1);
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.repeat =
        10; // High repeat count for continuous transmission while button is held
    instance->encoder.front = 0;
    instance->encoder.is_running = false;

    FURI_LOG_I(TAG, "Encoder allocated at %p", instance);
    return instance;
}

void subghz_protocol_encoder_kia_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    if(instance->encoder.upload) {
        free(instance->encoder.upload);
    }
    free(instance);
}

/**
 * Rebuild the 61-bit data packet with current button/counter values and recalculate CRC
 */
static void subghz_protocol_encoder_kia_update_data(SubGhzProtocolEncoderKIA* instance) {
    uint64_t data = 0;

    // Bits 56-59: Preserve from original capture
    data |= (instance->generic.data & 0x0F00000000000000ULL);

    // Bits 40-55: Counter (16 bits)
    data |= ((uint64_t)(instance->counter & 0xFFFF) << 40);

    // Bits 12-39: Serial (28 bits)
    data |= ((uint64_t)(instance->serial & 0x0FFFFFFF) << 12);

    // Bits 8-11: Button (4 bits)
    data |= ((uint64_t)(instance->button & 0x0F) << 8);

    // Bits 0-7: Calculate and set CRC
    uint8_t crc = kia_calculate_crc(data);
    data |= crc;

    instance->generic.data = data;

    FURI_LOG_I(
        TAG,
        "Data updated - Serial: 0x%07lX, Btn: 0x%X, Cnt: 0x%04X, CRC: 0x%02X",
        instance->serial,
        instance->button,
        instance->counter,
        crc);
    FURI_LOG_I(TAG, "Full data: 0x%016llX", instance->generic.data);
}

static void subghz_protocol_encoder_kia_get_upload(SubGhzProtocolEncoderKIA* instance) {
    furi_check(instance);
    size_t index = 0;

    for(uint8_t burst = 0; burst < KIA_TOTAL_BURSTS; burst++) {
        if(burst > 0) {
            instance->encoder.upload[index++] = level_duration_make(false, KIA_INTER_BURST_GAP_US);
        }

        for(int i = 0; i < KIA_HEADER_PULSES; i++) {
            bool is_high = (i % 2) == 0;
            instance->encoder.upload[index++] =
                level_duration_make(is_high, subghz_protocol_kia_const.te_short);
        }

        instance->encoder.upload[index++] =
            level_duration_make(true, subghz_protocol_kia_const.te_long);

        instance->encoder.upload[index++] =
            level_duration_make(false, subghz_protocol_kia_const.te_long);

        for(uint8_t bit_num = 0; bit_num < 59; bit_num++) {
            uint64_t bit_mask = 1ULL << (58 - bit_num);
            uint8_t current_bit = (instance->generic.data & bit_mask) ? 1 : 0;

            uint32_t duration = current_bit ? subghz_protocol_kia_const.te_long :
                                              subghz_protocol_kia_const.te_short;

            instance->encoder.upload[index++] = level_duration_make(true, duration);
            instance->encoder.upload[index++] = level_duration_make(false, duration);
        }

        instance->encoder.upload[index++] =
            level_duration_make(true, subghz_protocol_kia_const.te_long * 2);
    }

    instance->encoder.size_upload = index;
    instance->encoder.front = 0;

    FURI_LOG_I(
        TAG,
        "Upload built: %d bursts, size_upload=%zu, data_count_bit=%u, data=0x%016llX",
        KIA_TOTAL_BURSTS,
        instance->encoder.size_upload,
        instance->generic.data_count_bit,
        instance->generic.data);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_kia_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    instance->encoder.repeat = 10;

    SubGhzProtocolStatus res = SubGhzProtocolStatusError;
    do {
        // Read protocol name and validate
        FuriString* temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            furi_string_free(temp_str);
            break;
        }

        FURI_LOG_I(TAG, "Protocol: %s", furi_string_get_cstr(temp_str));

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

        // Read bit count
        uint32_t bit_count_temp;
        if(!flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1)) {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }

        FURI_LOG_I(TAG, "Bit count read: %lu", bit_count_temp);

        // Always use 61 bits for Kia V0
        instance->generic.data_count_bit = 61;

        // Read key data
        temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            FURI_LOG_E(TAG, "Missing Key");
            furi_string_free(temp_str);
            break;
        }

        const char* key_str = furi_string_get_cstr(temp_str);
        FURI_LOG_I(TAG, "Key string: %s", key_str);

        // Manual hex parsing
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

        // Verify CRC of the captured data
        if(!kia_verify_crc(key)) {
            FURI_LOG_W(TAG, "CRC mismatch in captured data - signal may be corrupted");
        }

        // Read or extract serial
        if(!flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1)) {
            instance->serial = (uint32_t)((key >> 12) & 0x0FFFFFFF);
            FURI_LOG_I(TAG, "Extracted serial: 0x%08lX", instance->serial);
        } else {
            FURI_LOG_I(TAG, "Read serial: 0x%08lX", instance->serial);
        }

        // Read or extract button
        uint32_t btn_temp;
        if(flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1)) {
            instance->button = (uint8_t)btn_temp;
            FURI_LOG_I(TAG, "Read button: 0x%02X", instance->button);
        } else {
            instance->button = (key >> 8) & 0x0F;
            FURI_LOG_I(TAG, "Extracted button: 0x%02X", instance->button);
        }

        // Read or extract counter
        uint32_t cnt_temp;
        if(flipper_format_read_uint32(flipper_format, "Cnt", &cnt_temp, 1)) {
            instance->counter = (uint16_t)cnt_temp;
            FURI_LOG_I(TAG, "Read counter: 0x%04X", instance->counter);
        } else {
            instance->counter = (key >> 40) & 0xFFFF;
            FURI_LOG_I(TAG, "Extracted counter: 0x%04X", instance->counter);
        }

        // Rebuild data with CRC recalculation
        subghz_protocol_encoder_kia_update_data(instance);

        if(!flipper_format_read_uint32(
               flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1)) {
            instance->encoder.repeat = 10;
            FURI_LOG_D(
                TAG, "Repeat not found in file, using default 10 for continuous transmission");
        }

        subghz_protocol_encoder_kia_get_upload(instance);

        instance->encoder.is_running = true;
        instance->encoder.front = 0;

        if(instance->generic.data == 0) {
            FURI_LOG_E(TAG, "Warning: data is 0!");
        }

        FURI_LOG_I(
            TAG,
            "Encoder initialized - will send %d bursts, repeat=%u, front=%zu",
            KIA_TOTAL_BURSTS,
            instance->encoder.repeat,
            instance->encoder.front);
        FURI_LOG_I(TAG, "Final data to transmit: 0x%016llX", instance->generic.data);
        res = SubGhzProtocolStatusOk;
    } while(false);

    return res;
}

void subghz_protocol_encoder_kia_stop(void* context) {
    if(!context) return;
    SubGhzProtocolEncoderKIA* instance = context;
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
}

LevelDuration subghz_protocol_encoder_kia_yield(void* context) {
    SubGhzProtocolEncoderKIA* instance = context;

    if(!instance || !instance->encoder.upload || instance->encoder.repeat == 0 ||
       !instance->encoder.is_running) {
        if(instance) {
            FURI_LOG_D(
                TAG,
                "Encoder yield stopped: repeat=%u, is_running=%d, upload=%p",
                instance->encoder.repeat,
                instance->encoder.is_running,
                instance->encoder.upload);
            instance->encoder.is_running = false;
        }
        return level_duration_reset();
    }

    if(instance->encoder.front >= instance->encoder.size_upload) {
        FURI_LOG_E(
            TAG,
            "Encoder front out of bounds: %zu >= %zu",
            instance->encoder.front,
            instance->encoder.size_upload);
        instance->encoder.is_running = false;
        instance->encoder.front = 0;
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

/**
 * Set button value and recalculate CRC
 */
void subghz_protocol_encoder_kia_set_button(void* context, uint8_t button) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    instance->button = button & 0x0F;
    subghz_protocol_encoder_kia_update_data(instance);
    subghz_protocol_encoder_kia_get_upload(instance);
    FURI_LOG_I(TAG, "Button set to 0x%X, upload rebuilt with new CRC", instance->button);
}

/**
 * Set counter value and recalculate CRC
 */
void subghz_protocol_encoder_kia_set_counter(void* context, uint16_t counter) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    instance->counter = counter;
    subghz_protocol_encoder_kia_update_data(instance);
    subghz_protocol_encoder_kia_get_upload(instance);
    FURI_LOG_I(TAG, "Counter set to 0x%04X, upload rebuilt with new CRC", instance->counter);
}

/**
 * Increment counter and recalculate CRC
 */
void subghz_protocol_encoder_kia_increment_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    instance->counter++;
    subghz_protocol_encoder_kia_update_data(instance);
    subghz_protocol_encoder_kia_get_upload(instance);
    FURI_LOG_I(
        TAG, "Counter incremented to 0x%04X, upload rebuilt with new CRC", instance->counter);
}

/**
 * Get current counter value
 */
uint16_t subghz_protocol_encoder_kia_get_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    return instance->counter;
}

/**
 * Get current button value
 */
uint8_t subghz_protocol_encoder_kia_get_button(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKIA* instance = context;
    return instance->button;
}

// ============================================================================
// DECODER IMPLEMENTATION
// ============================================================================

void* subghz_protocol_decoder_kia_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKIA* instance = malloc(sizeof(SubGhzProtocolDecoderKIA));
    instance->base.protocol = &kia_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_kia_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;
    free(instance);
}

void subghz_protocol_decoder_kia_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;
    instance->decoder.parser_step = KIADecoderStepReset;
}

void subghz_protocol_decoder_kia_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;

    switch(instance->decoder.parser_step) {
    case KIADecoderStepReset:
        if((level) && (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) <
                       subghz_protocol_kia_const.te_delta)) {
            instance->decoder.parser_step = KIADecoderStepCheckPreambula;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
        }
        break;

    case KIADecoderStepCheckPreambula:
        if(level) {
            if((DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) <
                subghz_protocol_kia_const.te_delta) ||
               (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) <
                subghz_protocol_kia_const.te_delta)) {
                instance->decoder.te_last = duration;
            } else {
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) <
             subghz_protocol_kia_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_short) <
             subghz_protocol_kia_const.te_delta)) {
            instance->header_count++;
            break;
        } else if(
            (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) <
             subghz_protocol_kia_const.te_delta) &&
            (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_long) <
             subghz_protocol_kia_const.te_delta)) {
            if(instance->header_count > 15) {
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 1;
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                FURI_LOG_I(
                    TAG, "Starting data decode after %u header pulses", instance->header_count);
            } else {
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;

    case KIADecoderStepSaveDuration:
        if(level) {
            if(duration >=
               (subghz_protocol_kia_const.te_long + subghz_protocol_kia_const.te_delta * 2UL)) {
                // End of transmission detected
                instance->decoder.parser_step = KIADecoderStepReset;

                if(instance->decoder.decode_count_bit ==
                   subghz_protocol_kia_const.min_count_bit_for_found) {
                    instance->generic.data = instance->decoder.decode_data;
                    instance->generic.data_count_bit = instance->decoder.decode_count_bit;

                    if(kia_verify_crc(instance->generic.data)) {
                        FURI_LOG_I(TAG, "Valid signal received with correct CRC");
                    } else {
                        FURI_LOG_W(TAG, "Signal received but CRC mismatch!");
                    }

                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                } else {
                    FURI_LOG_E(
                        TAG,
                        "Incomplete signal: only %u bits",
                        instance->decoder.decode_count_bit);
                }

                instance->decoder.decode_data = 0;
                instance->decoder.decode_count_bit = 0;
                break;
            } else {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = KIADecoderStepCheckDuration;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;

    case KIADecoderStepCheckDuration:
        if(!level) {
            if((DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_short) <
                subghz_protocol_kia_const.te_delta) &&
               (DURATION_DIFF(duration, subghz_protocol_kia_const.te_short) <
                subghz_protocol_kia_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 0);
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
            } else if(
                (DURATION_DIFF(instance->decoder.te_last, subghz_protocol_kia_const.te_long) <
                 subghz_protocol_kia_const.te_delta) &&
                (DURATION_DIFF(duration, subghz_protocol_kia_const.te_long) <
                 subghz_protocol_kia_const.te_delta)) {
                subghz_protocol_blocks_add_bit(&instance->decoder, 1);
                instance->decoder.parser_step = KIADecoderStepSaveDuration;
            } else {
                FURI_LOG_W(
                    TAG,
                    "Timing mismatch at bit %u. Last: %lu, Current: %lu",
                    instance->decoder.decode_count_bit,
                    instance->decoder.te_last,
                    duration);
                instance->decoder.parser_step = KIADecoderStepReset;
            }
        } else {
            instance->decoder.parser_step = KIADecoderStepReset;
        }
        break;
    }
}

static void subghz_protocol_kia_check_remote_controller(SubGhzBlockGeneric* instance) {
    instance->serial = (uint32_t)((instance->data >> 12) & 0x0FFFFFFF);
    instance->btn = (instance->data >> 8) & 0x0F;
    instance->cnt = (instance->data >> 40) & 0xFFFF;
}

uint8_t subghz_protocol_decoder_kia_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_kia_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;

    subghz_protocol_kia_check_remote_controller(&instance->generic);

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Preset", furi_string_get_cstr(preset->name)))
            break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Protocol", instance->generic.protocol_name))
            break;

        uint32_t bits = 61;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &bits, 1)) break;

        char key_str[20];
        snprintf(key_str, sizeof(key_str), "%016llX", instance->generic.data);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key_str)) break;

        if(!flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1))
            break;

        uint32_t temp = instance->generic.btn;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp, 1)) break;

        if(!flipper_format_write_uint32(flipper_format, "Cnt", &instance->generic.cnt, 1)) break;

        uint8_t crc = instance->generic.data & 0xFF;
        uint32_t crc_temp = crc;
        if(!flipper_format_write_uint32(flipper_format, "CRC", &crc_temp, 1)) break;

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_kia_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_kia_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_kia_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderKIA* instance = context;

    subghz_protocol_kia_check_remote_controller(&instance->generic);
    uint32_t code_found_hi = instance->generic.data >> 32;
    uint32_t code_found_lo = instance->generic.data & 0x00000000ffffffff;

    uint8_t received_crc = instance->generic.data & 0xFF;
    uint8_t calculated_crc = kia_calculate_crc(instance->generic.data);
    bool crc_valid = (received_crc == calculated_crc);

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Sn:%07lX Btn:%X Cnt:%04lX\r\n"
        "CRC:%02X %s\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        code_found_hi,
        code_found_lo,
        instance->generic.serial,
        instance->generic.btn,
        instance->generic.cnt,
        received_crc,
        crc_valid ? "(OK)" : "(FAIL)");
}
