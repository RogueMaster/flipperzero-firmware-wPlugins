#include "fiat_v1.h"
#include <string.h>

#define TAG "FiatProtocolV1"

// Magneti Marelli BSI keyfob protocol (PCF7946)
// Found on: Fiat Panda, Grande Punto (and possibly other Fiat/Lancia/Alfa ~2003-2012)
//
// RF: 433.92 MHz, Manchester encoding
// Two timing variants with identical frame structure:
//   Type A (e.g. Panda):        te_short ~260us, te_long ~520us
//   Type B (e.g. Grande Punto): te_short ~100us, te_long ~200us
// TE is auto-detected from preamble pulse averaging.
//
// Frame layout (104 bits = 13 bytes):
//   Bytes 0-1:  0xFFFF/0xFFFC preamble residue
//   Bytes 2-5:  Serial (32 bits)
//   Byte 6:     [Button:4 | Epoch:4]
//   Byte 7:     [Counter:5 | Scramble:2 | Fixed:1]
//   Bytes 8-12: Encrypted payload (40 bits)
//
// Original implementation by @lupettohf

#define FIAT_MARELLI_PREAMBLE_PULSE_MIN  35
#define FIAT_MARELLI_PREAMBLE_PULSE_MAX  450
#define FIAT_MARELLI_PREAMBLE_MIN        48
#define FIAT_MARELLI_MAX_DATA_BITS       104
#define FIAT_MARELLI_MIN_DATA_BITS       104
#define FIAT_MARELLI_GAP_TE_MULT         4
#define FIAT_MARELLI_SYNC_TE_MIN_MULT    4
#define FIAT_MARELLI_SYNC_TE_MAX_MULT    12
#define FIAT_MARELLI_RETX_GAP_MIN        5000
#define FIAT_MARELLI_RETX_SYNC_MIN       400
#define FIAT_MARELLI_RETX_SYNC_MAX       2800
#define FIAT_MARELLI_TE_TYPE_AB_BOUNDARY 180

static uint8_t fiat_marelli_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x03;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x01) : (crc << 1);
        }
    }
    return crc;
}

static const SubGhzBlockConst subghz_protocol_fiat_marelli_const = {
    .te_short = 260,
    .te_long = 520,
    .te_delta = 80,
    .min_count_bit_for_found = FIAT_MARELLI_MIN_DATA_BITS,
};

typedef enum {
    FiatMarelliDecoderStepReset = 0,
    FiatMarelliDecoderStepPreamble = 1,
    FiatMarelliDecoderStepSync = 2,
    FiatMarelliDecoderStepData = 3,
} FiatMarelliDecoderStep;

struct SubGhzProtocolDecoderFiatMarelli {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    ManchesterState manchester_state;
    uint8_t decoder_state;
    uint16_t preamble_count;

    uint8_t raw_data[13];
    uint8_t bit_count;
    uint32_t extra_data;

    uint32_t te_last;
    uint32_t te_sum;
    uint16_t te_count;
    uint32_t te_detected;
};

static void fiat_marelli_set_state(
    SubGhzProtocolDecoderFiatMarelli* instance,
    FiatMarelliDecoderStep new_state,
    const char* reason) {
    UNUSED(reason);
    instance->decoder_state = new_state;
}

static bool fiat_marelli_get_raw_bit(const uint8_t* raw, uint8_t bit_index) {
    return (raw[bit_index / 8] >> (7 - (bit_index % 8))) & 1U;
}

static void fiat_marelli_set_raw_bit(uint8_t* raw, uint8_t bit_index, bool value) {
    uint8_t byte_idx = bit_index / 8;
    uint8_t mask = 1U << (7 - (bit_index % 8));
    if(value) {
        raw[byte_idx] |= mask;
    } else {
        raw[byte_idx] &= (uint8_t)(~mask);
    }
}

static void fiat_marelli_rebuild_data_words_from_raw(SubGhzProtocolDecoderFiatMarelli* instance) {
    instance->generic.data = 0;
    instance->extra_data = 0;

    for(uint8_t i = 0; i < 64; i++) {
        instance->generic.data = (instance->generic.data << 1) |
                                 (fiat_marelli_get_raw_bit(instance->raw_data, i) ? 1U : 0U);
    }

    for(uint8_t i = 64; i < FIAT_MARELLI_MAX_DATA_BITS; i++) {
        instance->extra_data = (instance->extra_data << 1) |
                               (fiat_marelli_get_raw_bit(instance->raw_data, i) ? 1U : 0U);
    }

    instance->bit_count = FIAT_MARELLI_MAX_DATA_BITS;
    instance->generic.data_count_bit = FIAT_MARELLI_MAX_DATA_BITS;
}

static bool fiat_marelli_try_recover_tail_bits(SubGhzProtocolDecoderFiatMarelli* instance) {
    if(instance->bit_count >= FIAT_MARELLI_MAX_DATA_BITS) {
        return true;
    }

    if(instance->bit_count < (FIAT_MARELLI_MAX_DATA_BITS - 2)) {
        return false;
    }

    uint8_t missing_bits = FIAT_MARELLI_MAX_DATA_BITS - instance->bit_count;
    uint8_t variants = 1U << missing_bits;
    uint8_t match_count = 0;
    uint8_t matched_variant = 0;

    for(uint8_t variant = 0; variant < variants; variant++) {
        uint8_t trial[13];
        memcpy(trial, instance->raw_data, sizeof(trial));

        for(uint8_t i = 0; i < missing_bits; i++) {
            bool bit = ((variant >> (missing_bits - 1 - i)) & 1U) != 0;
            fiat_marelli_set_raw_bit(trial, instance->bit_count + i, bit);
        }

        uint8_t calc = fiat_marelli_crc8(trial, 12);
        if(calc == trial[12]) {
            match_count++;
            matched_variant = variant;
        }
    }

    if(match_count != 1) {
        return false;
    }

    for(uint8_t i = 0; i < missing_bits; i++) {
        bool bit = ((matched_variant >> (missing_bits - 1 - i)) & 1U) != 0;
        fiat_marelli_set_raw_bit(instance->raw_data, instance->bit_count + i, bit);
    }

    fiat_marelli_rebuild_data_words_from_raw(instance);
    return true;
}

static void fiat_marelli_prepare_data(SubGhzProtocolDecoderFiatMarelli* instance) {
    instance->bit_count = 0;
    instance->extra_data = 0;
    instance->generic.data = 0;
    instance->generic.data_count_bit = 0;
    memset(instance->raw_data, 0, sizeof(instance->raw_data));
    manchester_advance(
        instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
    fiat_marelli_set_state(instance, FiatMarelliDecoderStepData, "sync accepted");
}

static void fiat_marelli_rebuild_raw_data(SubGhzProtocolDecoderFiatMarelli* instance) {
    memset(instance->raw_data, 0, sizeof(instance->raw_data));

    uint64_t key = instance->generic.data;
    for(uint8_t i = 0; i < 8; i++) {
        instance->raw_data[i] = (uint8_t)(key >> (56 - i * 8));
    }

    uint8_t extra_bits =
        (instance->generic.data_count_bit > 64) ? (instance->generic.data_count_bit - 64) : 0;
    for(uint8_t i = 0; i < extra_bits && i < 32; i++) {
        uint8_t byte_idx = 8 + (i / 8);
        uint8_t bit_pos = 7 - (i % 8);
        if(instance->extra_data & (1UL << (extra_bits - 1 - i))) {
            instance->raw_data[byte_idx] |= (1U << bit_pos);
        }
    }

    instance->bit_count = instance->generic.data_count_bit;

    if(instance->bit_count >= 56) {
        instance->generic.serial =
            ((uint32_t)instance->raw_data[2] << 24) | ((uint32_t)instance->raw_data[3] << 16) |
            ((uint32_t)instance->raw_data[4] << 8) | ((uint32_t)instance->raw_data[5]);
        instance->generic.btn = (instance->raw_data[6] >> 4) & 0x0F;
        instance->generic.cnt = (instance->raw_data[7] >> 3) & 0x1F;
    }
}

static const char* fiat_marelli_button_name(uint8_t btn) {
    switch(btn) {
    case 0x8:
    case 0x7:
        return "Lock";
    case 0x0:
    case 0xB:
        return "Unlock";
    case 0xD:
        return "Trunk";
    default:
        return "Unknown";
    }
}

const SubGhzProtocolDecoder subghz_protocol_fiat_marelli_decoder = {
    .alloc = subghz_protocol_decoder_fiat_marelli_alloc,
    .free = subghz_protocol_decoder_fiat_marelli_free,
    .feed = subghz_protocol_decoder_fiat_marelli_feed,
    .reset = subghz_protocol_decoder_fiat_marelli_reset,
    .get_hash_data = subghz_protocol_decoder_fiat_marelli_get_hash_data,
    .serialize = subghz_protocol_decoder_fiat_marelli_serialize,
    .deserialize = subghz_protocol_decoder_fiat_marelli_deserialize,
    .get_string = subghz_protocol_decoder_fiat_marelli_get_string,
};

const SubGhzProtocolEncoder subghz_protocol_fiat_marelli_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};

const SubGhzProtocol fiat_v1_protocol = {
    .name = FIAT_MARELLI_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save,
    .decoder = &subghz_protocol_fiat_marelli_decoder,
    .encoder = &subghz_protocol_fiat_marelli_encoder,
};

void* subghz_protocol_decoder_fiat_marelli_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderFiatMarelli* instance =
        calloc(1, sizeof(SubGhzProtocolDecoderFiatMarelli));
    furi_check(instance);
    instance->base.protocol = &fiat_v1_protocol;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_fiat_marelli_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;
    free(instance);
}

void subghz_protocol_decoder_fiat_marelli_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;
    fiat_marelli_set_state(instance, FiatMarelliDecoderStepReset, "decoder reset");
    instance->preamble_count = 0;
    instance->bit_count = 0;
    instance->extra_data = 0;
    instance->te_last = 0;
    instance->te_sum = 0;
    instance->te_count = 0;
    instance->te_detected = 0;
    instance->generic.data = 0;
    instance->generic.data_count_bit = 0;
    memset(instance->raw_data, 0, sizeof(instance->raw_data));
    instance->manchester_state = ManchesterStateMid1;
}

void subghz_protocol_decoder_fiat_marelli_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;

    uint32_t te_short = instance->te_detected ?
                            instance->te_detected :
                            (uint32_t)subghz_protocol_fiat_marelli_const.te_short;
    uint32_t te_long = te_short * 2;
    uint32_t te_delta = te_short / 2;
    if(te_delta < 30) te_delta = 30;
    uint32_t diff;

    switch(instance->decoder_state) {
    case FiatMarelliDecoderStepReset:
        if(level) {
            if(duration >= FIAT_MARELLI_PREAMBLE_PULSE_MIN &&
               duration <= FIAT_MARELLI_PREAMBLE_PULSE_MAX) {
                fiat_marelli_set_state(instance, FiatMarelliDecoderStepPreamble, "preamble start");
                instance->preamble_count = 1;
                instance->te_sum = duration;
                instance->te_count = 1;
                instance->te_last = duration;
            }
        }
        break;

    case FiatMarelliDecoderStepPreamble:
        if(duration >= FIAT_MARELLI_PREAMBLE_PULSE_MIN &&
           duration <= FIAT_MARELLI_PREAMBLE_PULSE_MAX) {
            instance->preamble_count++;
            instance->te_sum += duration;
            instance->te_count++;
            instance->te_last = duration;
        } else if(!level) {
            if(instance->preamble_count >= FIAT_MARELLI_PREAMBLE_MIN && instance->te_count > 0) {
                instance->te_detected = instance->te_sum / instance->te_count;
                uint32_t gap_threshold = instance->te_detected * FIAT_MARELLI_GAP_TE_MULT;

                if(duration > gap_threshold) {
                    fiat_marelli_set_state(instance, FiatMarelliDecoderStepSync, "gap detected");
                    instance->te_last = duration;
                } else {
                    fiat_marelli_set_state(instance, FiatMarelliDecoderStepReset, "gap too short");
                }
            } else {
                fiat_marelli_set_state(
                    instance, FiatMarelliDecoderStepReset, "preamble too short");
            }
        } else {
            fiat_marelli_set_state(
                instance, FiatMarelliDecoderStepReset, "invalid preamble pulse");
        }
        break;

    case FiatMarelliDecoderStepSync: {
        uint32_t sync_min = instance->te_detected * FIAT_MARELLI_SYNC_TE_MIN_MULT;
        uint32_t sync_max = instance->te_detected * FIAT_MARELLI_SYNC_TE_MAX_MULT;

        if(level && duration >= sync_min && duration <= sync_max) {
            fiat_marelli_prepare_data(instance);
            instance->te_last = duration;
        } else {
            fiat_marelli_set_state(instance, FiatMarelliDecoderStepReset, "sync timing mismatch");
        }
        break;
    }

    case FiatMarelliDecoderStepData: {
        ManchesterEvent event = ManchesterEventReset;
        bool frame_complete = false;

        diff = (duration > te_short) ? (duration - te_short) : (te_short - duration);
        if(diff < te_delta) {
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        } else {
            diff = (duration > te_long) ? (duration - te_long) : (te_long - duration);
            if(diff < te_delta) {
                event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
            }
        }

        if(event != ManchesterEventReset) {
            bool data_bit = false;
            if(manchester_advance(
                   instance->manchester_state, event, &instance->manchester_state, &data_bit)) {
                uint32_t new_bit = data_bit ? 1U : 0U;

                if(instance->bit_count < FIAT_MARELLI_MAX_DATA_BITS) {
                    uint8_t byte_idx = instance->bit_count / 8;
                    uint8_t bit_pos = 7 - (instance->bit_count % 8);
                    if(new_bit) {
                        instance->raw_data[byte_idx] |= (1U << bit_pos);
                    }
                }

                if(instance->bit_count < 64) {
                    instance->generic.data = (instance->generic.data << 1) | new_bit;
                } else {
                    instance->extra_data = (instance->extra_data << 1) | new_bit;
                }

                instance->bit_count++;
                if(instance->bit_count >= FIAT_MARELLI_MAX_DATA_BITS) {
                    frame_complete = true;
                }
            }
        } else if(instance->bit_count >= (FIAT_MARELLI_MAX_DATA_BITS - 2)) {
            frame_complete = true;
        } else {
            fiat_marelli_set_state(
                instance, FiatMarelliDecoderStepReset, "invalid manchester timing");
        }

        if(frame_complete) {
            instance->generic.data_count_bit = instance->bit_count;

            if(!fiat_marelli_try_recover_tail_bits(instance)) {
                fiat_marelli_set_state(instance, FiatMarelliDecoderStepReset, "frame complete");
                instance->te_last = duration;
                break;
            }

            bool crc_ok = true;

            if(instance->bit_count >= FIAT_MARELLI_MAX_DATA_BITS) {
                uint8_t calc = fiat_marelli_crc8(instance->raw_data, 12);
                crc_ok = (calc == instance->raw_data[12]);
            }

            if(crc_ok) {
                FURI_LOG_D(TAG, "Frame accepted (%u bits, CRC OK)", instance->bit_count);
                instance->generic.serial = ((uint32_t)instance->raw_data[2] << 24) |
                                           ((uint32_t)instance->raw_data[3] << 16) |
                                           ((uint32_t)instance->raw_data[4] << 8) |
                                           ((uint32_t)instance->raw_data[5]);
                instance->generic.btn = (instance->raw_data[6] >> 4) & 0x0F;
                instance->generic.cnt = (instance->raw_data[7] >> 3) & 0x1F;

                if(instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }
            }

            fiat_marelli_set_state(instance, FiatMarelliDecoderStepReset, "frame complete");
        }

        instance->te_last = duration;
        break;
    }
    }
}

uint8_t subghz_protocol_decoder_fiat_marelli_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;
    SubGhzBlockDecoder decoder = {
        .decode_data = instance->generic.data,
        .decode_count_bit =
            instance->generic.data_count_bit > 64 ? 64 : instance->generic.data_count_bit,
    };
    uint8_t hash =
        subghz_protocol_blocks_get_hash_data(&decoder, (decoder.decode_count_bit / 8) + 1);
    uint32_t x = instance->extra_data;
    for(uint8_t i = 0; i < 4; i++) {
        hash ^= (uint8_t)(x >> (i * 8));
    }
    return hash;
}

SubGhzProtocolStatus subghz_protocol_decoder_fiat_marelli_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);
    if(ret == SubGhzProtocolStatusOk) {
        flipper_format_write_uint32(flipper_format, "Extra", &instance->extra_data, 1);

        uint32_t extra_bits =
            (instance->generic.data_count_bit > 64) ? (instance->generic.data_count_bit - 64) : 0;
        flipper_format_write_uint32(flipper_format, "Extra_bits", &extra_bits, 1);

        uint32_t te = instance->te_detected;
        flipper_format_write_uint32(flipper_format, "TE", &te, 1);
    }
    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_fiat_marelli_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic,
        flipper_format,
        subghz_protocol_fiat_marelli_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        if(instance->generic.data_count_bit != FIAT_MARELLI_MAX_DATA_BITS) {
            return SubGhzProtocolStatusErrorValueBitCount;
        }

        uint32_t extra = 0;
        if(flipper_format_read_uint32(flipper_format, "Extra", &extra, 1)) {
            instance->extra_data = extra;
        }

        uint32_t te = 0;
        if(flipper_format_read_uint32(flipper_format, "TE", &te, 1)) {
            instance->te_detected = te;
        }

        fiat_marelli_rebuild_raw_data(instance);
    }

    return ret;
}

void subghz_protocol_decoder_fiat_marelli_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderFiatMarelli* instance = context;

    uint8_t epoch = instance->raw_data[6] & 0x0F;
    uint8_t counter = (instance->raw_data[7] >> 3) & 0x1F;
    uint8_t scramble = (instance->raw_data[7] >> 1) & 0x03;
    uint8_t fixed = instance->raw_data[7] & 0x01;
    const char* crc_state = "N/A";
    uint8_t crc_value = instance->raw_data[12];

    if(instance->bit_count >= 104) {
        uint8_t calc = fiat_marelli_crc8(instance->raw_data, 12);
        crc_state = (calc == instance->raw_data[12]) ? "OK" : "FAIL";
    }

    furi_string_cat_printf(
        output,
        "%s %dbit\r\n"
        "Enc:%02X%02X%02X%02X%02X Scr:%02X\r\n"
        "Raw:%02X%02X Fixed:%X\r\n"
        "Sn:%08X Cnt:%02X\r\n"
        "Btn:%02X:[%s] Ep:%02X\r\n"
        "CRC:%02X [%s]\r\n",
        instance->generic.protocol_name,
        (int)instance->bit_count,
        instance->raw_data[8],
        instance->raw_data[9],
        instance->raw_data[10],
        instance->raw_data[11],
        instance->raw_data[12],
        (unsigned)scramble,
        instance->raw_data[6],
        instance->raw_data[7],
        (unsigned)fixed,
        (unsigned int)instance->generic.serial,
        (unsigned)counter,
        (unsigned)instance->generic.btn,
        fiat_marelli_button_name(instance->generic.btn),
        (unsigned)epoch,
        (unsigned)crc_value,
        crc_state);
}
