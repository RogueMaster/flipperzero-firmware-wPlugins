#include "fiat_v0.h"
#include "../protopirate_app_i.h"
#include <lib/toolbox/manchester_decoder.h>
#include <inttypes.h>

#define TAG                     "FiatProtocolV0"
#define FIAT_PROTOCOL_V0_NAME   "Fiat V0"
#define FIAT_V0_PREAMBLE_PAIRS  150
#define FIAT_V0_GAP_US          800
#define FIAT_V0_TOTAL_BURSTS    3
#define FIAT_V0_INTER_BURST_GAP 25000

static const SubGhzBlockConst subghz_protocol_fiat_v0_const = {
    .te_short = 200,
    .te_long = 400,
    .te_delta = 100,
    .min_count_bit_for_found = 64,
};

struct SubGhzProtocolDecoderFiatV0 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    ManchesterState manchester_state;
    uint8_t decoder_state;
    uint16_t preamble_count;
    uint32_t data_low;
    uint32_t data_high;
    uint8_t bit_count;
    uint32_t hop;
    uint32_t fix;
    uint8_t endbyte;
    uint8_t final_count;
    uint32_t te_last;
};

struct SubGhzProtocolEncoderFiatV0 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t hop;
    uint32_t fix;
    uint8_t endbyte;

    // Capacity of encoder.upload in LevelDuration elements.
    size_t upload_capacity;
};

typedef enum {
    FiatV0DecoderStepReset = 0,
    FiatV0DecoderStepPreamble = 1,
    FiatV0DecoderStepData = 2,
} FiatV0DecoderStep;

const SubGhzProtocolDecoder subghz_protocol_fiat_v0_decoder = {
    .alloc = subghz_protocol_decoder_fiat_v0_alloc,
    .free = subghz_protocol_decoder_fiat_v0_free,
    .feed = subghz_protocol_decoder_fiat_v0_feed,
    .reset = subghz_protocol_decoder_fiat_v0_reset,
    .get_hash_data = subghz_protocol_decoder_fiat_v0_get_hash_data,
    .serialize = subghz_protocol_decoder_fiat_v0_serialize,
    .deserialize = subghz_protocol_decoder_fiat_v0_deserialize,
    .get_string = subghz_protocol_decoder_fiat_v0_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_fiat_v0_encoder = {
    .alloc = subghz_protocol_encoder_fiat_v0_alloc,
    .free = subghz_protocol_encoder_fiat_v0_free,
    .deserialize = subghz_protocol_encoder_fiat_v0_deserialize,
    .stop = subghz_protocol_encoder_fiat_v0_stop,
    .yield = subghz_protocol_encoder_fiat_v0_yield,
};

const SubGhzProtocol fiat_protocol_v0 = {
    .name = FIAT_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_fiat_v0_decoder,
    .encoder = &subghz_protocol_fiat_v0_encoder,
};

// ============================================================================
// ENCODER IMPLEMENTATION
// ============================================================================

static size_t fiat_v0_encoder_calc_required_upload(void) {
    // Per burst:
    // - preamble: FIAT_V0_PREAMBLE_PAIRS pairs -> 2 elements each
    // - data: 64 bits Manchester -> 2 elements per bit
    // - endbyte: 7 bits Manchester -> 2 elements per bit
    // - trailer: 1 element (extended low)
    const size_t per_burst = (FIAT_V0_PREAMBLE_PAIRS * 2) + (64 * 2) + (7 * 2) + 1;
    // Inter-burst gap is a single element between bursts.
    return (FIAT_V0_TOTAL_BURSTS * per_burst) +
           (FIAT_V0_TOTAL_BURSTS > 0 ? (FIAT_V0_TOTAL_BURSTS - 1) : 0);
}

static void
    fiat_v0_encoder_ensure_upload_capacity(SubGhzProtocolEncoderFiatV0* instance, size_t required) {
    furi_check(instance);
    furi_check(required <= instance->upload_capacity);

    LevelDuration* new_upload =
        realloc(instance->encoder.upload, required * sizeof(LevelDuration));
    furi_check(new_upload);
    instance->encoder.upload = new_upload;
    instance->upload_capacity = required;
}

void* subghz_protocol_encoder_fiat_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderFiatV0* instance = calloc(1, sizeof(SubGhzProtocolEncoderFiatV0));
    furi_check(instance);

    instance->base.protocol = &fiat_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 0;
    instance->upload_capacity = fiat_v0_encoder_calc_required_upload();
    instance->encoder.upload = calloc(instance->upload_capacity, sizeof(LevelDuration));
    furi_check(instance->encoder.upload);
    instance->encoder.is_running = false;

    return instance;
}

void subghz_protocol_encoder_fiat_v0_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFiatV0* instance = context;
    if(instance->encoder.upload) {
        free(instance->encoder.upload);
    }
    free(instance);
}

static void subghz_protocol_encoder_fiat_v0_get_upload(SubGhzProtocolEncoderFiatV0* instance) {
    furi_check(instance);
    size_t index = 0;

    const size_t required = fiat_v0_encoder_calc_required_upload();
    fiat_v0_encoder_ensure_upload_capacity(instance, required);

    uint32_t te_short = subghz_protocol_fiat_v0_const.te_short;

    FURI_LOG_I(
        TAG,
        "Building upload: hop=0x%08lX, fix=0x%08lX, endbyte=0x%02X",
        instance->hop,
        instance->fix,
        instance->endbyte & 0x7F);

    for(uint8_t burst = 0; burst < FIAT_V0_TOTAL_BURSTS; burst++) {
        if(burst > 0) {
            instance->encoder.upload[index++] =
                level_duration_make(false, FIAT_V0_INTER_BURST_GAP);
        }

        // Preamble: alternating short pulses
        for(int i = 0; i < FIAT_V0_PREAMBLE_PAIRS; i++) {
            instance->encoder.upload[index++] = level_duration_make(true, te_short);
            instance->encoder.upload[index++] = level_duration_make(false, te_short);
        }

        // Extend last LOW to create the gap (~FIAT_V0_GAP_US)
        instance->encoder.upload[index - 1] = level_duration_make(false, FIAT_V0_GAP_US);

        // Combine hop and fix into 64-bit data
        uint64_t data = ((uint64_t)instance->hop << 32) | instance->fix;

        // Manchester encode 64 bits of data
        for(int bit = 63; bit >= 0; bit--) {
            bool curr_bit = (data >> bit) & 1;

            if(curr_bit) {
                instance->encoder.upload[index++] = level_duration_make(true, te_short);
                instance->encoder.upload[index++] = level_duration_make(false, te_short);
            } else {
                instance->encoder.upload[index++] = level_duration_make(false, te_short);
                instance->encoder.upload[index++] = level_duration_make(true, te_short);
            }
        }

        // Manchester encode 7 bits of endbyte (bits 6:0) - signal has 71 total bits
        uint8_t endbyte = (uint8_t)(instance->endbyte & 0x7F);
        for(int bit = 6; bit >= 0; bit--) {
            bool curr_bit = (endbyte >> bit) & 1;

            if(curr_bit) {
                instance->encoder.upload[index++] = level_duration_make(true, te_short);
                instance->encoder.upload[index++] = level_duration_make(false, te_short);
            } else {
                instance->encoder.upload[index++] = level_duration_make(false, te_short);
                instance->encoder.upload[index++] = level_duration_make(true, te_short);
            }
        }

        // End with extended LOW (will be followed by inter-burst gap or end)
        instance->encoder.upload[index++] = level_duration_make(false, te_short * 4);
    }

    furi_check(index <= instance->upload_capacity);
    instance->encoder.size_upload = index;
    instance->encoder.front = 0;

    FURI_LOG_I(TAG, "Upload built: %zu elements", instance->encoder.size_upload);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_fiat_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderFiatV0* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    instance->encoder.repeat = 10;

    flipper_format_rewind(flipper_format);

    FuriString* temp_str = furi_string_alloc();
    furi_check(temp_str);

    do {
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            break;
        }

        if(!furi_string_equal(temp_str, instance->base.protocol->name)) {
            FURI_LOG_E(TAG, "Wrong protocol: %s", furi_string_get_cstr(temp_str));
            break;
        }

        uint32_t bit_count_temp = 0;
        if(flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1)) {
            // This protocol transmits 71 bits: 64-bit key + 7-bit endbyte.
            // Let's do according to what's read, (64 or 71), else 71
            if(bit_count_temp == 64 || bit_count_temp == 71) {
                instance->generic.data_count_bit = bit_count_temp;
            } else {
                FURI_LOG_E(
                    TAG,
                    "Inconsistent Bit value of %d was defaulted to 71",
                    instance->generic.data_count_bit);
                instance->generic.data_count_bit = 71;
            }
        } else {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }

        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            FURI_LOG_E(TAG, "Missing Key");
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
                nibble = (uint8_t)(c - '0');
            } else if(c >= 'A' && c <= 'F') {
                nibble = (uint8_t)(c - 'A' + 10);
            } else if(c >= 'a' && c <= 'f') {
                nibble = (uint8_t)(c - 'a' + 10);
            } else {
                break;
            }

            key = (key << 4) | nibble;
            hex_pos++;
        }

        if(hex_pos != 16) {
            FURI_LOG_E(TAG, "Key parse error: expected 16 hex nibbles, got %u", (unsigned)hex_pos);
            break;
        }

        instance->generic.data = key;
        instance->hop = (uint32_t)(key >> 32);
        instance->fix = (uint32_t)(key & 0xFFFFFFFF);

        uint32_t btn_temp = 0;
        if(flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1)) {
            instance->endbyte = (uint8_t)(btn_temp & 0x7F);
        } else {
            instance->endbyte = 0;
        }

        instance->generic.btn = instance->endbyte;
        instance->generic.cnt = instance->hop;
        instance->generic.serial = instance->fix;

        uint32_t repeat_temp = 0;
        if(flipper_format_read_uint32(flipper_format, "Repeat", &repeat_temp, 1)) {
            instance->encoder.repeat = repeat_temp;
        } else {
            instance->encoder.repeat = 10;
        }

        subghz_protocol_encoder_fiat_v0_get_upload(instance);
        instance->encoder.is_running = true;

        FURI_LOG_I(
            TAG,
            "Encoder ready: hop=0x%08lX, fix=0x%08lX, endbyte=0x%02X",
            instance->hop,
            instance->fix,
            instance->endbyte);

        ret = SubGhzProtocolStatusOk;
    } while(false);

    furi_string_free(temp_str);
    return ret;
}

void subghz_protocol_encoder_fiat_v0_stop(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFiatV0* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_fiat_v0_yield(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFiatV0* instance = context;

    if(!instance->encoder.is_running || instance->encoder.repeat == 0) {
        instance->encoder.is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];

    if(++instance->encoder.front == instance->encoder.size_upload) {
        instance->encoder.repeat--;
        instance->encoder.front = 0;
    }

    return ret;
}

// ============================================================================
// DECODER IMPLEMENTATION
// ============================================================================

void* subghz_protocol_decoder_fiat_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderFiatV0* instance = calloc(1, sizeof(SubGhzProtocolDecoderFiatV0));
    furi_check(instance);
    instance->base.protocol = &fiat_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_fiat_v0_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    free(instance);
}

void subghz_protocol_decoder_fiat_v0_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    instance->decoder.parser_step = FiatV0DecoderStepReset;
    instance->decoder_state = 0;
    instance->preamble_count = 0;
    instance->data_low = 0;
    instance->data_high = 0;
    instance->bit_count = 0;
    instance->hop = 0;
    instance->fix = 0;
    instance->endbyte = 0;
    instance->final_count = 0;
    instance->te_last = 0;
    instance->manchester_state = ManchesterStateMid1;
}

// Helper function to reset decoder to data state with proper Manchester initialization
static void
    fiat_v0_decoder_enter_data_state(SubGhzProtocolDecoderFiatV0* instance, uint32_t duration) {
    instance->decoder_state = FiatV0DecoderStepData;
    instance->preamble_count = 0;
    instance->data_low = 0;
    instance->data_high = 0;
    instance->bit_count = 0;
    instance->te_last = duration;
    // Critical: Reset Manchester state when entering data mode
    manchester_advance(
        instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
}

void subghz_protocol_decoder_fiat_v0_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    uint32_t te_short = (uint32_t)subghz_protocol_fiat_v0_const.te_short;
    uint32_t te_long = (uint32_t)subghz_protocol_fiat_v0_const.te_long;
    uint32_t te_delta = (uint32_t)subghz_protocol_fiat_v0_const.te_delta;
    uint32_t gap_threshold = FIAT_V0_GAP_US;
    uint32_t diff;

    switch(instance->decoder_state) {
    case FiatV0DecoderStepReset:
        if(!level) {
            return;
        }
        if(duration < te_short) {
            diff = te_short - duration;
        } else {
            diff = duration - te_short;
        }
        if(diff < te_delta) {
            instance->data_low = 0;
            instance->data_high = 0;
            instance->decoder_state = FiatV0DecoderStepPreamble;
            instance->te_last = duration;
            instance->preamble_count = 0;
            instance->bit_count = 0;
            manchester_advance(
                instance->manchester_state,
                ManchesterEventReset,
                &instance->manchester_state,
                NULL);
        }
        break;

    case FiatV0DecoderStepPreamble:
        if(level) {
            // HIGH pulse during preamble - just track timing
            if(duration < te_short) {
                diff = te_short - duration;
            } else {
                diff = duration - te_short;
            }
            if(diff < te_delta) {
                instance->preamble_count++;
                instance->te_last = duration;
            } else {
                instance->decoder_state = FiatV0DecoderStepReset;
            }
            return;
        }

        // LOW pulse - check if it's the gap after preamble
        if(duration < te_short) {
            diff = te_short - duration;
        } else {
            diff = duration - te_short;
        }

        if(diff < te_delta) {
            // Normal short LOW - continue preamble
            instance->preamble_count++;
            instance->te_last = duration;
        } else {
            // Not a short pulse - check if it's the gap
            if(instance->preamble_count >= 0x96) {
                // We have enough preamble, check for gap
                if(duration < gap_threshold) {
                    diff = gap_threshold - duration;
                } else {
                    diff = duration - gap_threshold;
                }
                if(diff < te_delta) {
                    // Valid gap detected - transition to data state
                    fiat_v0_decoder_enter_data_state(instance, duration);
                    return;
                }
            }
            // Invalid timing or not enough preamble
            instance->decoder_state = FiatV0DecoderStepReset;
        }

        // Also check for gap even with valid short timing if we have enough preamble
        if(instance->preamble_count >= 0x96 &&
           instance->decoder_state == FiatV0DecoderStepPreamble) {
            if(duration < gap_threshold) {
                diff = gap_threshold - duration;
            } else {
                diff = duration - gap_threshold;
            }
            if(diff < te_delta) {
                // Valid gap detected - transition to data state
                fiat_v0_decoder_enter_data_state(instance, duration);
                return;
            }
        }
        break;

    case FiatV0DecoderStepData:
        ManchesterEvent event = ManchesterEventReset;
        if(duration < te_short) {
            diff = te_short - duration;
            if(diff < te_delta) {
                event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
            }
        } else {
            diff = duration - te_short;
            if(diff < te_delta) {
                event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
            } else {
                if(duration < te_long) {
                    diff = te_long - duration;
                } else {
                    diff = duration - te_long;
                }
                if(diff < te_delta) {
                    event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
                }
            }
        }

        if(event != ManchesterEventReset) {
            bool data_bit_bool;
            if(manchester_advance(
                   instance->manchester_state,
                   event,
                   &instance->manchester_state,
                   &data_bit_bool)) {
                uint32_t new_bit = data_bit_bool ? 1 : 0;

                uint32_t carry = (instance->data_low >> 31) & 1;
                instance->data_low = (instance->data_low << 1) | new_bit;
                instance->data_high = (instance->data_high << 1) | carry;

                instance->bit_count++;

                if(instance->bit_count == 0x40) {
                    instance->fix = instance->data_low;
                    instance->hop = instance->data_high;
                    FURI_LOG_D(
                        TAG,
                        "Bit 64: fix=0x%08lX, hop=0x%08lX, clearing data_low/data_high",
                        instance->fix,
                        instance->hop);
                    instance->data_low = 0;
                    instance->data_high = 0;
                }

#ifndef REMOVE_LOGS
                if(instance->bit_count > 0x40 && instance->bit_count <= 0x47) {
                    uint8_t endbyte_bit_num = instance->bit_count - 0x40;
                    uint8_t endbyte_bit_index = endbyte_bit_num - 1;
                    uint8_t data_low_byte = (uint8_t)instance->data_low;
                    char binary_str[9] = {0};
                    for(int i = 7; i >= 0; i--) {
                        binary_str[7 - i] = (data_low_byte & (1 << i)) ? '1' : '0';
                    }
                    FURI_LOG_D(
                        TAG,
                        "Bit %d (endbyte bit %d/%d): new_bit=%lu, data_low=0x%08lX (0x%02X), binary=0b%s",
                        instance->bit_count,
                        endbyte_bit_index,
                        endbyte_bit_num - 1,
                        (unsigned long)new_bit,
                        instance->data_low,
                        data_low_byte,
                        binary_str);
                }
#endif
                if(instance->bit_count == 0x47) {
#ifndef REMOVE_LOGS
                    uint8_t data_low_byte = (uint8_t)instance->data_low;
                    char binary_str[9] = {0};
                    for(int i = 7; i >= 0; i--) {
                        binary_str[7 - i] = (data_low_byte & (1 << i)) ? '1' : '0';
                    }
                    FURI_LOG_D(
                        TAG,
                        "EXTRACTING AT BIT 71: bit_count=%d, data_low=0x%08lX (0x%02X), binary=0b%s, accumulated %d bits after bit 64",
                        instance->bit_count,
                        instance->data_low,
                        data_low_byte,
                        binary_str,
                        instance->bit_count - 0x40);
#endif
                    instance->final_count = instance->bit_count;
                    instance->endbyte = (uint8_t)instance->data_low;

                    FURI_LOG_D(
                        TAG,
                        "EXTRACTED ENDBYTE: endbyte=0x%02X (decimal=%d), expected=0x0D (13)",
                        instance->endbyte,
                        instance->endbyte & 0x7F);

                    instance->generic.data = ((uint64_t)instance->hop << 32) | instance->fix;
                    instance->generic.data_count_bit = 71;
                    instance->generic.serial = instance->fix;
                    instance->generic.btn = instance->endbyte;
                    instance->generic.cnt = instance->hop;

                    if(instance->base.callback) {
                        instance->base.callback(&instance->base, instance->base.context);
                    }

                    instance->data_low = 0;
                    instance->data_high = 0;
                    instance->bit_count = 0;
                    instance->decoder_state = FiatV0DecoderStepReset;
                }
            }
        } else {
            if(instance->bit_count == 0x47) {
                // We have exactly 71 bits - extract endbyte
                uint8_t data_low_byte = (uint8_t)instance->data_low;
                instance->endbyte = data_low_byte;

                FURI_LOG_D(
                    TAG,
                    "GAP PATH EXTRACTION (71 bits): bit_count=%d, endbyte=0x%02X",
                    instance->bit_count,
                    instance->endbyte & 0x7F);

                instance->generic.data = ((uint64_t)instance->hop << 32) | instance->fix;
                instance->generic.data_count_bit = 71;
                instance->generic.serial = instance->fix;
                instance->generic.btn = instance->endbyte;
                instance->generic.cnt = instance->hop;

                if(instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }

                instance->data_low = 0;
                instance->data_high = 0;
                instance->bit_count = 0;
                instance->decoder_state = FiatV0DecoderStepReset;
            } else if(instance->bit_count < 0x40) {
                instance->decoder_state = FiatV0DecoderStepReset;
            }
        }
        instance->te_last = duration;
        break;
    }
}

uint8_t subghz_protocol_decoder_fiat_v0_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    SubGhzBlockDecoder decoder = {
        .decode_data = instance->generic.data,
        .decode_count_bit = instance->generic.data_count_bit};
    return subghz_protocol_blocks_get_hash_data(&decoder, (decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_fiat_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;

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
        snprintf(key_str, sizeof(key_str), "%08lX%08lX", instance->hop, instance->fix);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key_str)) break;

        uint32_t temp = instance->hop;
        if(!flipper_format_write_uint32(flipper_format, "Cnt", &temp, 1)) break;

        if(!flipper_format_write_uint32(flipper_format, "Serial", &instance->fix, 1)) break;

        temp = instance->endbyte;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp, 1)) break;

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_fiat_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;
    return subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_fiat_v0_const.min_count_bit_for_found);
}

void subghz_protocol_decoder_fiat_v0_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderFiatV0* instance = context;

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Key:%08lX%08lX\r\n"
        "Hop:%08lX\r\n"
        "Sn:%08lX\r\n"
        "EndByte:%02X\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        instance->hop,
        instance->fix,
        instance->hop,
        instance->fix,
        instance->endbyte & 0x7F);
}
