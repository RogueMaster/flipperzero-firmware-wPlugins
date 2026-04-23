#include "psa.h"

#define TAG "PSAProtocol"

static const SubGhzBlockConst subghz_protocol_psa_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 128,
};

#define PSA_TE_SHORT_125        0x7d
#define PSA_TE_LONG_250         0xfa
#define PSA_TE_END_1000         1000
#define PSA_TE_END_500          500
#define PSA_TOLERANCE_99        99
#define PSA_TOLERANCE_100       100
#define PSA_TOLERANCE_49        0x31
#define PSA_TOLERANCE_50        0x32
#define PSA_PATTERN_THRESHOLD_1 0x46
#define PSA_PATTERN_THRESHOLD_2 0x45
#define PSA_MAX_BITS            0x79
#define PSA_KEY1_BITS           0x40
#define PSA_KEY2_BITS           0x50
#define PSA_BUFFER_SIZE         48
#define PSA_TE_LONG_300         0x12c

#define TEA_DELTA  0x9E3779B9U
#define TEA_ROUNDS 32

typedef struct {
    uint32_t s0[TEA_ROUNDS];
    uint32_t s1[TEA_ROUNDS];
} PsaTeaSchedule;

static void psa_tea_build_schedule(const uint32_t* key, PsaTeaSchedule* out) {
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t sum0 = (uint32_t)((uint64_t)i * TEA_DELTA);
        uint32_t sum1 = (uint32_t)((uint64_t)(i + 1) * TEA_DELTA);
        out->s0[i] = key[sum0 & 3] + sum0;
        out->s1[i] = key[(sum1 >> 11) & 3] + sum1;
    }
}

static inline void
    psa_tea_encrypt_with_schedule(uint32_t* v0, uint32_t* v1, const PsaTeaSchedule* sched) {
    for(int i = 0; i < TEA_ROUNDS; i++) {
        *v0 += (sched->s0[i] ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
        *v1 += (sched->s1[i] ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
    }
}

#define PSA_BF1_CONST_U4 0x0E0F5C41U
#define PSA_BF1_CONST_U5 0x0F5C4123U

static const uint32_t PSA_BF1_KEY_SCHEDULE[4] = {
    0x4A434915U,
    0xD6743C2BU,
    0x1F29D308U,
    0xE6B79A64U,
};

static const uint32_t PSA_BF2_KEY_SCHEDULE[4] = {
    0x4039C240U,
    0xEDA92CABU,
    0x4306C02AU,
    0x02192A04U,
};

#define PSA_BF1_START 0x23000000U
#define PSA_BF1_END   0x24000000U
#define PSA_BF2_START 0xF3000000U
#define PSA_BF2_END   0xF4000000U

typedef enum {
    PSADecoderState0 = 0,
    PSADecoderState1 = 1,
    PSADecoderState2 = 2,
    PSADecoderState3 = 3,
    PSADecoderState4 = 4,
} PSADecoderState;

struct SubGhzProtocolDecoderPSA {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;

    uint32_t state;
    uint32_t prev_duration;

    uint32_t decode_data_low;
    uint32_t decode_data_high;
    uint8_t decode_count_bit;

    uint32_t seed;
    uint32_t key1_low;
    uint32_t key1_high;
    uint16_t validation_field;
    uint32_t key2_low;
    uint32_t key2_high;

    uint32_t status_flag;
    uint16_t decrypted;
    uint8_t mode_serialize;

    uint16_t pattern_counter;
    ManchesterState manchester_state;

    uint8_t decrypted_button;
    uint32_t decrypted_serial;
    uint32_t decrypted_counter;
    uint16_t decrypted_crc;
    uint32_t decrypted_seed;
    uint8_t decrypted_type;
};

#ifdef ENABLE_EMULATE_FEATURE
struct SubGhzProtocolEncoderPSA {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t key1_low;
    uint32_t key1_high;
    uint16_t validation_field;
    uint32_t key2_low;
    uint32_t counter;
    uint8_t button;
    uint8_t type;
    uint8_t seed;
    uint8_t mode;
    uint32_t serial;
    uint16_t crc;
    bool is_running;
};
#endif
const SubGhzProtocolDecoder subghz_protocol_psa_decoder = {
    .alloc = subghz_protocol_decoder_psa_alloc,
    .free = subghz_protocol_decoder_psa_free,
    .feed = subghz_protocol_decoder_psa_feed,
    .reset = subghz_protocol_decoder_psa_reset,
    .get_hash_data = subghz_protocol_decoder_psa_get_hash_data,
    .serialize = subghz_protocol_decoder_psa_serialize,
    .deserialize = subghz_protocol_decoder_psa_deserialize,
    .get_string = subghz_protocol_decoder_psa_get_string,
};
#ifdef ENABLE_EMULATE_FEATURE
const SubGhzProtocolEncoder subghz_protocol_psa_encoder = {
    .alloc = subghz_protocol_encoder_psa_alloc,
    .free = subghz_protocol_encoder_psa_free,
    .deserialize = subghz_protocol_encoder_psa_deserialize,
    .stop = subghz_protocol_encoder_psa_stop,
    .yield = subghz_protocol_encoder_psa_yield,
};
#else
const SubGhzProtocolEncoder subghz_protocol_psa_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};
#endif
const SubGhzProtocol psa_protocol = {
    .name = PSA_PROTOCOL_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Load,
    .decoder = &subghz_protocol_psa_decoder,
    .encoder = &subghz_protocol_psa_encoder,
};

static void psa_setup_byte_buffer(
    uint8_t* buffer,
    uint32_t key1_low,
    uint32_t key1_high,
    uint32_t key2_low);
static void psa_calculate_checksum(uint8_t* buffer);
static uint8_t psa_calculate_tea_crc(uint32_t v0, uint32_t v1);
#ifdef ENABLE_EMULATE_FEATURE
static void psa_tea_encrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key);
#endif
static void psa_unpack_tea_result_to_buffer(uint8_t* buffer, uint32_t v0, uint32_t v1);
static bool parse_hex_key_str(const char* str, uint64_t* out);

#ifdef ENABLE_EMULATE_FEATURE
static void psa_second_stage_xor_encrypt(uint8_t* buffer) {
    uint8_t E6 = buffer[8];
    uint8_t E7 = buffer[9];

    uint8_t P[6];
    P[0] = buffer[2];
    P[1] = buffer[3];
    P[2] = buffer[4];
    P[3] = buffer[5];
    P[4] = buffer[6];
    P[5] = buffer[7];

    uint8_t E5 = P[5] ^ E7 ^ E6;
    uint8_t E0 = P[2] ^ E5;
    uint8_t E2 = P[4] ^ E0;
    uint8_t E4 = P[3] ^ E2;
    uint8_t E3 = P[0] ^ E5;
    uint8_t E1 = P[1] ^ E3;

    buffer[2] = E0;
    buffer[3] = E1;
    buffer[4] = E2;
    buffer[5] = E3;
    buffer[6] = E4;
    buffer[7] = E5;
}

static void psa_build_buffer_mode23(
    SubGhzProtocolEncoderPSA* instance,
    uint8_t* buffer,
    uint8_t* preserve_buffer01) {
    FURI_LOG_I(TAG, "=== MODE 0x23 ENCRYPTION ===");
    FURI_LOG_I(
        TAG,
        "Input: Ser:%06lX Cnt:%08lX CRC:%02X Btn:%01X",
        (unsigned long)instance->serial,
        (unsigned long)instance->counter,
        (unsigned int)instance->crc,
        (unsigned int)instance->button);

    memset(buffer, 0, PSA_BUFFER_SIZE);

    buffer[2] = (uint8_t)((instance->serial >> 16) & 0xFF);
    buffer[3] = (uint8_t)((instance->serial >> 8) & 0xFF);
    buffer[4] = (uint8_t)(instance->serial & 0xFF);
    buffer[5] = (uint8_t)((instance->counter >> 8) & 0xFF);
    buffer[6] = (uint8_t)(instance->counter & 0xFF);
    buffer[7] = (uint8_t)(instance->crc & 0xFF);
    buffer[8] = (uint8_t)(instance->button & 0xF);

    uint8_t original_buffer9 = 0;
#ifndef REMOVE_LOGS
    uint8_t original_buffer8 = 0;
#endif
    bool has_original_key2 = (instance->key2_low != 0);
    if(has_original_key2) {
        original_buffer9 = (uint8_t)(instance->key2_low & 0xFF);
#ifndef REMOVE_LOGS
        original_buffer8 = (uint8_t)((instance->key2_low >> 8) & 0xFF);
#endif
        buffer[9] = original_buffer9;
        FURI_LOG_D(
            TAG,
            "Original Key2: 0x%04X, buffer[8]=0x%02X buffer[9]=0x%02X, preserving buffer[9]",
            (unsigned int)instance->key2_low,
            original_buffer8,
            original_buffer9);
    } else {
        FURI_LOG_D(TAG, "No original Key2, will find valid buffer[9]");
    }

    FURI_LOG_D(
        TAG,
        "Plaintext buffer[2-9]: %02X %02X %02X %02X %02X %02X %02X %02X",
        buffer[2],
        buffer[3],
        buffer[4],
        buffer[5],
        buffer[6],
        buffer[7],
        buffer[8],
        buffer[9]);

    uint8_t initial_plaintext[6];
    initial_plaintext[0] = buffer[2];
    initial_plaintext[1] = buffer[3];
    initial_plaintext[2] = buffer[4];
    initial_plaintext[3] = buffer[5];
    initial_plaintext[4] = buffer[6];
    initial_plaintext[5] = buffer[7];
    uint8_t initial_button = buffer[8] & 0xF;

    bool found = false;
    uint8_t buffer9_to_use = has_original_key2 ? original_buffer9 : 0;
    uint8_t buffer9_end = has_original_key2 ? original_buffer9 + 1 : 255;

    for(uint8_t buffer9_try = buffer9_to_use; buffer9_try < buffer9_end && !found; buffer9_try++) {
        for(uint8_t buffer8_high_try = 0; buffer8_high_try < 16 && !found; buffer8_high_try++) {
            buffer[2] = initial_plaintext[0];
            buffer[3] = initial_plaintext[1];
            buffer[4] = initial_plaintext[2];
            buffer[5] = initial_plaintext[3];
            buffer[6] = initial_plaintext[4];
            buffer[7] = initial_plaintext[5];
            buffer[8] = initial_button | (buffer8_high_try << 4);
            buffer[9] = buffer9_try;

            psa_second_stage_xor_encrypt(buffer);

            psa_calculate_checksum(buffer);
            uint8_t checksum_after = buffer[11];
            uint8_t key2_high_after = checksum_after & 0xF0;

            uint8_t validation = (checksum_after ^ buffer[8]) & 0xF0;
            if(validation == 0) {
                buffer[8] = (buffer[8] & 0x0F) | key2_high_after;
                buffer[13] = buffer[9] ^ buffer[8];
                found = true;
                FURI_LOG_D(
                    TAG,
                    "Found valid Key2: buffer[8]=0x%02X buffer[9]=0x%02X",
                    buffer[8],
                    buffer[9]);
                break;
            }
        }
    }

    if(!found) {
        FURI_LOG_W(TAG, "Brute force failed, using default approach");
        buffer[2] = initial_plaintext[0];
        buffer[3] = initial_plaintext[1];
        buffer[4] = initial_plaintext[2];
        buffer[5] = initial_plaintext[3];
        buffer[6] = initial_plaintext[4];
        buffer[7] = initial_plaintext[5];
        buffer[8] = initial_button;
        buffer[9] = has_original_key2 ? original_buffer9 : 0x23;

        psa_second_stage_xor_encrypt(buffer);
        psa_calculate_checksum(buffer);
        uint8_t checksum_after = buffer[11];
        uint8_t key2_high_after = checksum_after & 0xF0;
        buffer[8] = (buffer[8] & 0x0F) | key2_high_after;
        buffer[13] = buffer[9] ^ buffer[8];
    }

    if(preserve_buffer01 != NULL) {
        buffer[0] = preserve_buffer01[0];
        buffer[1] = preserve_buffer01[1];
        FURI_LOG_D(
            TAG, "Preserved buffer[0-1] from original Key1: %02X %02X", buffer[0], buffer[1]);
    } else {
        buffer[0] = buffer[2] ^ buffer[6];
        buffer[1] = buffer[3] ^ buffer[7];
        FURI_LOG_D(TAG, "Derived buffer[0-1]: %02X %02X", buffer[0], buffer[1]);
    }

    FURI_LOG_I(
        TAG,
        "Encrypted buffer[0-9]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        buffer[0],
        buffer[1],
        buffer[2],
        buffer[3],
        buffer[4],
        buffer[5],
        buffer[6],
        buffer[7],
        buffer[8],
        buffer[9]);
}

static void psa_build_buffer_mode36(
    SubGhzProtocolEncoderPSA* instance,
    uint8_t* buffer,
    uint8_t* preserve_buffer01) {
    FURI_LOG_I(TAG, "=== MODE 0x36 ENCRYPTION ===");
    FURI_LOG_I(
        TAG,
        "Input: Ser:%06lX Cnt:%08lX CRC:%02X Btn:%01X",
        (unsigned long)instance->serial,
        (unsigned long)instance->counter,
        (unsigned int)instance->crc,
        (unsigned int)instance->button);

    memset(buffer, 0, PSA_BUFFER_SIZE);

    uint32_t v0 = ((instance->serial & 0xFFFFFF) << 8) | ((instance->counter >> 24) & 0xFF);
    uint32_t v1 = ((instance->counter & 0xFFFFFF) << 8) | ((instance->button & 0xF) << 4) |
                  (instance->crc & 0xFF);

    FURI_LOG_D(
        TAG, "Packed v0:0x%08lX v1:0x%08lX (before CRC)", (unsigned long)v0, (unsigned long)v1);

    uint8_t crc = psa_calculate_tea_crc(v0, v1);
    v1 = (v1 & 0xFFFFFF00) | crc;

    FURI_LOG_D(
        TAG, "Calculated CRC: 0x%02X, v1 after CRC: 0x%08lX", (unsigned int)crc, (unsigned long)v1);

    uint32_t bf_counter = PSA_BF1_START | (instance->serial & 0xFFFFFF);
    FURI_LOG_D(TAG, "BF counter: 0x%08lX (BF1_START | serial)", (unsigned long)bf_counter);

    uint32_t working_key[4];

    uint32_t wk2 = PSA_BF1_CONST_U4;
    uint32_t wk3 = bf_counter;
    psa_tea_encrypt(&wk2, &wk3, PSA_BF1_KEY_SCHEDULE);

    uint32_t wk0 = (bf_counter << 8) | 0x0E;
    uint32_t wk1 = PSA_BF1_CONST_U5;
    psa_tea_encrypt(&wk0, &wk1, PSA_BF1_KEY_SCHEDULE);

    working_key[0] = wk0;
    working_key[1] = wk1;
    working_key[2] = wk2;
    working_key[3] = wk3;

    psa_tea_encrypt(&v0, &v1, working_key);

    FURI_LOG_D(TAG, "TEA encrypted v0:0x%08lX v1:0x%08lX", (unsigned long)v0, (unsigned long)v1);
    FURI_LOG_D(
        TAG,
        "Working key: %08lX %08lX %08lX %08lX",
        (unsigned long)working_key[0],
        (unsigned long)working_key[1],
        (unsigned long)working_key[2],
        (unsigned long)working_key[3]);

    psa_unpack_tea_result_to_buffer(buffer, v0, v1);

    if(preserve_buffer01 != NULL) {
        buffer[0] = preserve_buffer01[0];
        buffer[1] = preserve_buffer01[1];
        FURI_LOG_D(
            TAG, "Preserved buffer[0-1] from original Key1: %02X %02X", buffer[0], buffer[1]);
    } else {
        buffer[0] = buffer[2] ^ buffer[6];
        buffer[1] = buffer[3] ^ buffer[7];
        FURI_LOG_D(TAG, "Derived buffer[0-1]: %02X %02X", buffer[0], buffer[1]);
    }

    FURI_LOG_I(
        TAG,
        "Encrypted buffer[0-9]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        buffer[0],
        buffer[1],
        buffer[2],
        buffer[3],
        buffer[4],
        buffer[5],
        buffer[6],
        buffer[7],
        buffer[8],
        buffer[9]);
}

static void psa_encoder_build_upload(SubGhzProtocolEncoderPSA* instance) {
    furi_check(instance);

    FURI_LOG_I(TAG, "=== ENCODER BUILD UPLOAD ===");
    FURI_LOG_I(
        TAG,
        "Mode: 0x%02X, Serial:%06lX Counter:%08lX CRC:%02X Button:%01X",
        (unsigned int)instance->mode,
        (unsigned long)instance->serial,
        (unsigned long)instance->counter,
        (unsigned int)instance->crc,
        (unsigned int)instance->button);

    uint8_t buffer[PSA_BUFFER_SIZE] = {0};

    uint8_t preserve_buffer01[2] = {0};
    uint8_t* preserve_ptr = NULL;

    if(instance->key1_low != 0 || instance->key1_high != 0) {
        uint8_t orig_buffer[PSA_BUFFER_SIZE] = {0};
        psa_setup_byte_buffer(
            orig_buffer, instance->key1_low, instance->key1_high, instance->key2_low);
        preserve_buffer01[0] = orig_buffer[0];
        preserve_buffer01[1] = orig_buffer[1];
        preserve_ptr = preserve_buffer01;
        FURI_LOG_D(
            TAG,
            "Original Key1: %08lX%08lX, preserving buffer[0-1]: %02X %02X",
            (unsigned long)instance->key1_high,
            (unsigned long)instance->key1_low,
            preserve_buffer01[0],
            preserve_buffer01[1]);
    } else {
        FURI_LOG_D(TAG, "No original Key1, will derive buffer[0-1]");
    }

    if(instance->mode == 0x23) {
        psa_build_buffer_mode23(instance, buffer, preserve_ptr);
    } else if(instance->mode == 0x36) {
        psa_build_buffer_mode36(instance, buffer, preserve_ptr);
    } else {
        FURI_LOG_E(TAG, "Unknown mode: 0x%02X", instance->mode);
        return;
    }

    uint32_t key1_high = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
                         ((uint32_t)buffer[2] << 8) | (uint32_t)buffer[3];
    uint32_t key1_low = ((uint32_t)buffer[4] << 24) | ((uint32_t)buffer[5] << 16) |
                        ((uint32_t)buffer[6] << 8) | (uint32_t)buffer[7];
    uint16_t validation_field = ((uint16_t)buffer[8] << 8) | (uint16_t)buffer[9];

    FURI_LOG_I(TAG, "=== ENCRYPTED KEYS ===");
    FURI_LOG_I(TAG, "Key1: %08lX%08lX", (unsigned long)key1_high, (unsigned long)key1_low);
    FURI_LOG_I(TAG, "Key2: %04X", (unsigned int)validation_field);

    size_t index = 0;
    uint32_t te = PSA_TE_LONG_250;

    for(int i = 0; i < 80; i++) {
        if(index >= instance->encoder.size_upload - 2) break;
        instance->encoder.upload[index++] = level_duration_make(true, te);
        instance->encoder.upload[index++] = level_duration_make(false, te);
    }

    uint32_t te_long_transition = subghz_protocol_psa_const.te_long;
    if(index < instance->encoder.size_upload - 3) {
        instance->encoder.upload[index++] = level_duration_make(false, te);
        instance->encoder.upload[index++] = level_duration_make(true, te_long_transition);
        instance->encoder.upload[index++] = level_duration_make(false, te);
    }

    uint64_t key1_data = ((uint64_t)key1_high << 32) | key1_low;
    for(int bit = 63; bit >= 0; bit--) {
        if(index >= instance->encoder.size_upload - 2) break;
        bool bit_value = (key1_data >> bit) & 1;
        if(bit_value) {
            instance->encoder.upload[index++] = level_duration_make(true, te);
            instance->encoder.upload[index++] = level_duration_make(false, te);
        } else {
            instance->encoder.upload[index++] = level_duration_make(false, te);
            instance->encoder.upload[index++] = level_duration_make(true, te);
        }
    }

    for(int bit = 15; bit >= 0; bit--) {
        if(index >= instance->encoder.size_upload - 2) break;
        bool bit_value = (validation_field >> bit) & 1;
        if(bit_value) {
            instance->encoder.upload[index++] = level_duration_make(true, te);
            instance->encoder.upload[index++] = level_duration_make(false, te);
        } else {
            instance->encoder.upload[index++] = level_duration_make(false, te);
            instance->encoder.upload[index++] = level_duration_make(true, te);
        }
    }

    uint32_t end_duration = PSA_TE_END_1000;
    if(index < instance->encoder.size_upload - 1) {
        instance->encoder.upload[index++] = level_duration_make(true, end_duration);
        instance->encoder.upload[index++] = level_duration_make(false, end_duration);
    }

    instance->encoder.size_upload = index;
    instance->encoder.front = 0;
    instance->encoder.repeat = 10;

    FURI_LOG_I(TAG, "=== TRANSMISSION PARAMETERS ===");
    FURI_LOG_I(
        TAG,
        "TE: %lu, TE_LONG_TRANS: %lu, TE_END: %lu",
        (unsigned long)te,
        (unsigned long)te_long_transition,
        (unsigned long)end_duration);
    FURI_LOG_I(
        TAG,
        "Upload size: %zu levels, Repeat: %d",
        instance->encoder.size_upload,
        instance->encoder.repeat);
    FURI_LOG_I(TAG, "Key1 data (64-bit): %016llX", (unsigned long long)key1_data);
    FURI_LOG_I(TAG, "Key2 data (16-bit): %04X", (unsigned int)validation_field);
    FURI_LOG_I(TAG, "=== END ENCODER BUILD ===");
}

void* subghz_protocol_encoder_psa_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderPSA* instance = malloc(sizeof(SubGhzProtocolEncoderPSA));

    if(instance) {
        memset(instance, 0, sizeof(SubGhzProtocolEncoderPSA));
        instance->base.protocol = &psa_protocol;
        instance->generic.protocol_name = instance->base.protocol->name;

        instance->encoder.size_upload = 600;
        instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
        instance->encoder.repeat = 10;
        instance->encoder.front = 0;
        instance->encoder.is_running = false;
        instance->is_running = false;
    }

    return instance;
}

void subghz_protocol_encoder_psa_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderPSA* instance = context;

    if(instance->encoder.upload) {
        free(instance->encoder.upload);
    }
    free(instance);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_psa_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderPSA* instance = context;

    FURI_LOG_I(TAG, "=== ENCODER DESERIALIZE ===");

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    FuriString* temp_str = furi_string_alloc();
    uint8_t hex_buffer[8];

    do {
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            if(!flipper_format_read_hex(flipper_format, "Key1", hex_buffer, 8)) {
                FURI_LOG_E(TAG, "Failed to read Key1");
                break;
            }
            instance->key1_low = ((uint32_t)hex_buffer[0] << 24) |
                                 ((uint32_t)hex_buffer[1] << 16) | ((uint32_t)hex_buffer[2] << 8) |
                                 (uint32_t)hex_buffer[3];
            instance->key1_high = ((uint32_t)hex_buffer[4] << 24) |
                                  ((uint32_t)hex_buffer[5] << 16) |
                                  ((uint32_t)hex_buffer[6] << 8) | (uint32_t)hex_buffer[7];
        } else {
            const char* key_str = furi_string_get_cstr(temp_str);
            uint64_t key1 = 0;
            size_t str_len = strlen(key_str);
            for(size_t i = 0; i < str_len && i < 16; i++) {
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
                    break;
                }
                key1 = (key1 << 4) | nibble;
            }
            instance->key1_low = (uint32_t)(key1 & 0xFFFFFFFF);
            instance->key1_high = (uint32_t)((key1 >> 32) & 0xFFFFFFFF);
        }

        flipper_format_rewind(flipper_format);
        if(flipper_format_read_string(flipper_format, "Key_2", temp_str)) {
            const char* key2_str = furi_string_get_cstr(temp_str);
            uint64_t key2 = 0;
            size_t str_len = strlen(key2_str);
            for(size_t i = 0; i < str_len && i < 16; i++) {
                char c = key2_str[i];
                if(c == ' ') continue;
                uint8_t nibble;
                if(c >= '0' && c <= '9') {
                    nibble = c - '0';
                } else if(c >= 'A' && c <= 'F') {
                    nibble = c - 'A' + 10;
                } else if(c >= 'a' && c <= 'f') {
                    nibble = c - 'a' + 10;
                } else {
                    break;
                }
                key2 = (key2 << 4) | nibble;
            }
            instance->key2_low = (uint32_t)(key2 & 0xFFFFFFFF);
            instance->validation_field = (uint16_t)(key2 & 0xFFFF);
        } else {
            uint32_t val_field_val;
            if(flipper_format_read_uint32(flipper_format, "ValidationField", &val_field_val, 1)) {
                instance->validation_field = (uint16_t)(val_field_val & 0xFFFF);
                instance->key2_low = instance->validation_field;
            } else {
                uint8_t val_field[2];
                if(flipper_format_read_hex(flipper_format, "ValidationField", val_field, 2)) {
                    instance->validation_field = ((uint16_t)val_field[0] << 8) | val_field[1];
                    instance->key2_low = instance->validation_field;
                } else {
                    FURI_LOG_E(TAG, "ValidationField not found");
                    break;
                }
            }
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1)) {
            FURI_LOG_E(TAG, "Serial not found");
            break;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Cnt", &instance->counter, 1)) {
            FURI_LOG_E(TAG, "Counter not found");
            break;
        }

        flipper_format_rewind(flipper_format);
        uint32_t btn_val = 0;
        if(!flipper_format_read_uint32(flipper_format, "Btn", &btn_val, 1)) {
            FURI_LOG_E(TAG, "Button not found");
            break;
        }
        instance->button = (uint8_t)(btn_val & 0xFF);

        flipper_format_rewind(flipper_format);
        uint32_t type_val = 0;
        if(!flipper_format_read_uint32(flipper_format, "Type", &type_val, 1)) {
            FURI_LOG_E(TAG, "Type not found");
            break;
        }
        instance->type = (uint8_t)(type_val & 0xFF);

        flipper_format_rewind(flipper_format);
        uint32_t crc_val = 0;
        if(!flipper_format_read_uint32(flipper_format, "CRC", &crc_val, 1)) {
            FURI_LOG_E(TAG, "CRC not found");
            break;
        }
        instance->crc = (uint16_t)(crc_val & 0xFFFF);

        flipper_format_rewind(flipper_format);
        uint32_t seed_val = 0;
        if(!flipper_format_read_uint32(flipper_format, "Seed", &seed_val, 1)) {
            FURI_LOG_E(TAG, "Seed not found");
            break;
        }
        instance->seed = (uint8_t)(seed_val & 0xFF);

        instance->mode = instance->type;
        if(instance->mode == 0x23 || instance->mode == 0) {
            instance->mode = 0x23;
        } else if(instance->mode == 0x36) {
            instance->mode = 0x36;
        } else {
            instance->mode = 0x23;
        }

        FURI_LOG_I(TAG, "=== LOADED VALUES ===");
        FURI_LOG_I(
            TAG,
            "Key1: %08lX%08lX",
            (unsigned long)instance->key1_high,
            (unsigned long)instance->key1_low);
        FURI_LOG_I(
            TAG,
            "Key2: %04X (validation_field: %04X)",
            (unsigned int)instance->key2_low,
            (unsigned int)instance->validation_field);
        FURI_LOG_I(
            TAG,
            "Serial: %06lX, Counter: %08lX, CRC: %02X, Button: %01X, Type/Mode: 0x%02X",
            (unsigned long)instance->serial,
            (unsigned long)instance->counter,
            (unsigned int)instance->crc,
            (unsigned int)instance->button,
            (unsigned int)instance->mode);

        psa_encoder_build_upload(instance);

        instance->is_running = true;
        ret = SubGhzProtocolStatusOk;
    } while(false);

    furi_string_free(temp_str);
    return ret;
}

void subghz_protocol_encoder_psa_stop(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderPSA* instance = context;
    instance->is_running = false;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_psa_yield(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderPSA* instance = context;

    if(!instance->is_running || instance->encoder.size_upload == 0) {
        instance->is_running = false;
        return level_duration_reset();
    }

    LevelDuration ret = instance->encoder.upload[instance->encoder.front];
    instance->encoder.front++;

    if(instance->encoder.front >= instance->encoder.size_upload) {
        instance->encoder.front = 0;
        instance->encoder.repeat--;
        if(instance->encoder.repeat <= 0) {
            instance->is_running = false;
        }
    }

    return ret;
}
#endif
static uint32_t psa_abs_diff(uint32_t a, uint32_t b) {
    if(a < b) {
        return b - a;
    } else {
        return a - b;
    }
}

static void psa_setup_byte_buffer(
    uint8_t* buffer,
    uint32_t key1_low,
    uint32_t key1_high,
    uint32_t key2_low) {
    for(int i = 0; i < 8; i++) {
        int shift = i * 8;
        uint8_t byte_val;
        if(shift < 32) {
            byte_val = (uint8_t)((key1_low >> shift) & 0xFF);
        } else {
            byte_val = (uint8_t)((key1_high >> (shift - 32)) & 0xFF);
        }
        buffer[7 - i] = byte_val;
    }
    buffer[9] = (uint8_t)(key2_low & 0xFF);
    buffer[8] = (uint8_t)((key2_low >> 8) & 0xFF);
}

static bool psa_direct_xor_allowed_by_key2(uint8_t key2_high_byte) {
    uint8_t lo = key2_high_byte & 0xf;
    if(lo < 3) return true;
    if(lo < 7 && (key2_high_byte & 0xc) != 0) return true;
    return false;
}

static void psa_calculate_checksum(uint8_t* buffer) {
    uint32_t checksum = 0;
    for(int i = 2; i < 8; i++) {
        checksum += (buffer[i] & 0xF) + ((buffer[i] >> 4) & 0xF);
    }
    buffer[11] = (uint8_t)((checksum * 0x10) & 0xFF);
}

static void psa_copy_reverse(uint8_t* temp, uint8_t* source) {
    temp[0] = source[5];
    temp[1] = source[4];
    temp[2] = source[3];
    temp[3] = source[2];
    temp[4] = source[9];
    temp[5] = source[8];
    temp[6] = source[7];
    temp[7] = source[6];
}

static void psa_second_stage_xor_decrypt(uint8_t* buffer) {
    uint8_t temp[8];
    psa_copy_reverse(temp, buffer);
    buffer[2] = temp[0] ^ temp[6];
    buffer[3] = temp[2] ^ temp[0];
    buffer[4] = temp[6] ^ temp[3];
    buffer[5] = temp[7] ^ temp[1];
    buffer[6] = temp[3] ^ temp[1];
    buffer[7] = temp[6] ^ temp[4] ^ temp[5];
}

#ifdef ENABLE_EMULATE_FEATURE
static void psa_tea_encrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key) {
    uint32_t sum = 0;
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t k_idx1 = sum & 3;
        uint32_t temp = key[k_idx1] + sum;
        sum = sum + TEA_DELTA;
        *v0 = *v0 + (temp ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
        uint32_t k_idx2 = (sum >> 11) & 3;
        temp = key[k_idx2] + sum;
        *v1 = *v1 + (temp ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
    }
}
#endif

static inline void psa_tea_decrypt(uint32_t* v0, uint32_t* v1, const uint32_t* key) {
    uint32_t sum = TEA_DELTA * TEA_ROUNDS;
    for(int i = 0; i < TEA_ROUNDS; i++) {
        uint32_t k_idx2 = (sum >> 11) & 3;
        uint32_t temp = key[k_idx2] + sum;
        sum = sum - TEA_DELTA;
        *v1 = *v1 - (temp ^ (((*v0 >> 5) ^ (*v0 << 4)) + *v0));
        uint32_t k_idx1 = sum & 3;
        temp = key[k_idx1] + sum;
        *v0 = *v0 - (temp ^ (((*v1 >> 5) ^ (*v1 << 4)) + *v1));
    }
}

static void psa_prepare_tea_data(uint8_t* buffer, uint32_t* w0, uint32_t* w1) {
    *w0 = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[2] << 24) | ((uint32_t)buffer[4] << 8) |
          (uint32_t)buffer[5];
    *w1 = ((uint32_t)buffer[7] << 16) | ((uint32_t)buffer[6] << 24) | ((uint32_t)buffer[8] << 8) |
          (uint32_t)buffer[9];
}

static uint8_t psa_calculate_tea_crc(uint32_t v0, uint32_t v1) {
    uint32_t crc = ((v0 >> 24) & 0xFF) + ((v0 >> 16) & 0xFF) + ((v0 >> 8) & 0xFF) + (v0 & 0xFF);
    crc += ((v1 >> 24) & 0xFF) + ((v1 >> 16) & 0xFF) + ((v1 >> 8) & 0xFF);
    return (uint8_t)(crc & 0xFF);
}

static uint16_t psa_calculate_crc16_bf2(uint8_t* buffer, int length) {
    uint16_t crc = 0;
    for(int i = 0; i < length; i++) {
        crc = crc ^ ((uint16_t)buffer[i] << 8);
        for(int j = 0; j < 8; j++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc = crc << 1;
            }
            crc = crc & 0xFFFF;
        }
    }
    return crc & 0xFFFF;
}

static void psa_unpack_tea_result_to_buffer(uint8_t* buffer, uint32_t v0, uint32_t v1) {
    buffer[2] = (uint8_t)((v0 >> 24) & 0xFF);
    buffer[3] = (uint8_t)((v0 >> 16) & 0xFF);
    buffer[4] = (uint8_t)((v0 >> 8) & 0xFF);
    buffer[5] = (uint8_t)(v0 & 0xFF);
    buffer[6] = (uint8_t)((v1 >> 24) & 0xFF);
    buffer[7] = (uint8_t)((v1 >> 16) & 0xFF);
    buffer[8] = (uint8_t)((v1 >> 8) & 0xFF);
    buffer[9] = (uint8_t)(v1 & 0xFF);
}

static void psa_extract_fields_mode23(uint8_t* buffer, SubGhzProtocolDecoderPSA* instance) {
    instance->decrypted_button = buffer[8] & 0xF;
    instance->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                                 (uint32_t)buffer[4];
    instance->decrypted_counter = (uint32_t)buffer[6] | ((uint32_t)buffer[5] << 8);
    instance->decrypted_crc = (uint16_t)buffer[7];
    instance->decrypted_type = 0x23;
    instance->decrypted_seed = instance->decrypted_serial;
}

static void psa_extract_fields_mode36(uint8_t* buffer, SubGhzProtocolDecoderPSA* instance) {
    instance->decrypted_button = (buffer[5] >> 4) & 0xF;
    instance->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                                 (uint32_t)buffer[4];
    instance->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                                  (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    instance->decrypted_crc = (uint16_t)buffer[9];
    instance->decrypted_type = 0x36;
    instance->decrypted_seed = instance->decrypted_serial;
}

static bool psa_brute_force_decrypt_bf1(
    SubGhzProtocolDecoderPSA* instance,
    uint8_t* buffer,
    uint32_t w0,
    uint32_t w1) {
    PsaTeaSchedule bf1_sched;
    psa_tea_build_schedule(PSA_BF1_KEY_SCHEDULE, &bf1_sched);

    for(uint32_t counter = PSA_BF1_START; counter < PSA_BF1_END; counter++) {
        uint32_t wk2 = PSA_BF1_CONST_U4;
        uint32_t wk3 = counter;
        psa_tea_encrypt_with_schedule(&wk2, &wk3, &bf1_sched);

        uint32_t wk0 = (counter << 8) | 0x0E;
        uint32_t wk1 = PSA_BF1_CONST_U5;
        psa_tea_encrypt_with_schedule(&wk0, &wk1, &bf1_sched);

        uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            uint8_t crc = psa_calculate_tea_crc(dec_v0, dec_v1);
            if(crc == (dec_v1 & 0xFF)) {
                psa_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
                psa_extract_fields_mode36(buffer, instance);
                return true;
            }
        }
    }
    return false;
}

static bool psa_brute_force_decrypt_bf2(
    SubGhzProtocolDecoderPSA* instance,
    uint8_t* buffer,
    uint32_t w0,
    uint32_t w1) {
    for(uint32_t counter = PSA_BF2_START; counter < PSA_BF2_END; counter++) {
        uint32_t working_key[4] = {
            PSA_BF2_KEY_SCHEDULE[0] ^ counter,
            PSA_BF2_KEY_SCHEDULE[1] ^ counter,
            PSA_BF2_KEY_SCHEDULE[2] ^ counter,
            PSA_BF2_KEY_SCHEDULE[3] ^ counter,
        };

        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            psa_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);

            uint8_t crc_buffer[6] = {
                (uint8_t)((dec_v0 >> 24) & 0xFF),
                (uint8_t)((dec_v0 >> 8) & 0xFF),
                (uint8_t)((dec_v0 >> 16) & 0xFF),
                (uint8_t)(dec_v0 & 0xFF),
                (uint8_t)((dec_v1 >> 24) & 0xFF),
                (uint8_t)((dec_v1 >> 16) & 0xFF),
            };
            uint16_t crc16 = psa_calculate_crc16_bf2(crc_buffer, 6);

            uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);

            if(crc16 == expected_crc) {
                psa_extract_fields_mode36(buffer, instance);
                return true;
            }
        }
    }
    return false;
}

static void psa_fill_bf_state_from_buffer(PsaBfState* state, uint8_t* buffer) {
    state->decrypted_button = (buffer[5] >> 4) & 0xF;
    state->decrypted_serial = ((uint32_t)buffer[3] << 8) | ((uint32_t)buffer[2] << 16) |
                              (uint32_t)buffer[4];
    state->decrypted_counter = ((uint32_t)buffer[7] << 8) | ((uint32_t)buffer[6] << 16) |
                               (uint32_t)buffer[8] | (((uint32_t)buffer[5] & 0xF) << 24);
    state->decrypted_crc = (uint16_t)buffer[9];
    state->decrypted_seed = state->decrypted_serial;
    state->decrypted_type = 0x36;
}

#define PSA_BF_PROGRESS_INTERVAL 4096U

void psa_brute_force_run(PsaBfState* state) {
    uint8_t buffer[48] = {0};
    psa_setup_byte_buffer(buffer, state->key1_low, state->key1_high, state->key2_low);
    uint32_t w0, w1;
    psa_prepare_tea_data(buffer, &w0, &w1);

    state->progress_current = 0;
    state->progress_total = (PSA_BF1_END - PSA_BF1_START) + (PSA_BF2_END - PSA_BF2_START);
    state->status = PSA_BF_STATUS_RUNNING;

    PsaTeaSchedule bf1_sched;
    psa_tea_build_schedule(PSA_BF1_KEY_SCHEDULE, &bf1_sched);

    for(uint32_t counter = PSA_BF1_START; counter < PSA_BF1_END; counter++) {
        if(state->cancel) {
            state->status = PSA_BF_STATUS_CANCELLED;
            return;
        }
        if((counter & (PSA_BF_PROGRESS_INTERVAL - 1)) == 0) {
            state->progress_current = counter - PSA_BF1_START;
        }

        uint32_t wk2 = PSA_BF1_CONST_U4;
        uint32_t wk3 = counter;
        psa_tea_encrypt_with_schedule(&wk2, &wk3, &bf1_sched);
        uint32_t wk0 = (counter << 8) | 0x0E;
        uint32_t wk1 = PSA_BF1_CONST_U5;
        psa_tea_encrypt_with_schedule(&wk0, &wk1, &bf1_sched);
        uint32_t working_key[4] = {wk0, wk1, wk2, wk3};

        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            uint8_t crc = psa_calculate_tea_crc(dec_v0, dec_v1);
            if(crc == (dec_v1 & 0xFF)) {
                psa_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
                psa_fill_bf_state_from_buffer(state, buffer);
                state->progress_current = counter - PSA_BF1_START;
                state->status = PSA_BF_STATUS_FOUND;
                return;
            }
        }
    }

    state->progress_current = PSA_BF1_END - PSA_BF1_START;

    for(uint32_t counter = PSA_BF2_START; counter < PSA_BF2_END; counter++) {
        if(state->cancel) {
            state->status = PSA_BF_STATUS_CANCELLED;
            return;
        }
        if((counter & (PSA_BF_PROGRESS_INTERVAL - 1)) == 0) {
            state->progress_current = (PSA_BF1_END - PSA_BF1_START) + (counter - PSA_BF2_START);
        }

        uint32_t working_key[4] = {
            PSA_BF2_KEY_SCHEDULE[0] ^ counter,
            PSA_BF2_KEY_SCHEDULE[1] ^ counter,
            PSA_BF2_KEY_SCHEDULE[2] ^ counter,
            PSA_BF2_KEY_SCHEDULE[3] ^ counter,
        };
        uint32_t dec_v0 = w0;
        uint32_t dec_v1 = w1;
        psa_tea_decrypt(&dec_v0, &dec_v1, working_key);

        if((counter & 0xFFFFFF) == (dec_v0 >> 8)) {
            psa_unpack_tea_result_to_buffer(buffer, dec_v0, dec_v1);
            uint8_t crc_buffer[6] = {
                (uint8_t)((dec_v0 >> 24) & 0xFF),
                (uint8_t)((dec_v0 >> 8) & 0xFF),
                (uint8_t)((dec_v0 >> 16) & 0xFF),
                (uint8_t)(dec_v0 & 0xFF),
                (uint8_t)((dec_v1 >> 24) & 0xFF),
                (uint8_t)((dec_v1 >> 16) & 0xFF),
            };
            uint16_t crc16 = psa_calculate_crc16_bf2(crc_buffer, 6);
            uint16_t expected_crc = (uint16_t)(dec_v1 & 0xFFFF);
            if(crc16 == expected_crc) {
                psa_fill_bf_state_from_buffer(state, buffer);
                state->progress_current =
                    (PSA_BF1_END - PSA_BF1_START) + (counter - PSA_BF2_START);
                state->status = PSA_BF_STATUS_FOUND;
                return;
            }
        }
    }

    state->status = PSA_BF_STATUS_NOT_FOUND;
}

int32_t psa_brute_force_thread_entry(void* arg) {
    PsaBfState* state = arg;
    psa_brute_force_run(state);

    if(state->on_done) {
        state->on_done(state->on_done_ctx);
    }
    return 0;
}

static bool parse_hex_key_str(const char* str, uint64_t* out) {
    *out = 0;
    size_t len = str ? strlen(str) : 0;
    if(len > 16) len = 16;
    for(size_t i = 0; i < len; i++) {
        char c = str[i];
        if(c == ' ') continue;
        uint8_t nibble;
        if(c >= '0' && c <= '9')
            nibble = c - '0';
        else if(c >= 'A' && c <= 'F')
            nibble = c - 'A' + 10;
        else if(c >= 'a' && c <= 'f')
            nibble = c - 'a' + 10;
        else
            return false;
        *out = (*out << 4) | nibble;
    }
    return true;
}

bool psa_bf_state_from_flipper_format(PsaBfState* state, FlipperFormat* ff) {
    furi_check(state);
    furi_check(ff);
    FuriString* temp = furi_string_alloc();
    bool ok = false;
    do {
        flipper_format_rewind(ff);
        if(!flipper_format_read_string(ff, "Key", temp)) break;
        uint64_t key1 = 0;
        if(!parse_hex_key_str(furi_string_get_cstr(temp), &key1)) break;
        state->key1_low = (uint32_t)(key1 & 0xFFFFFFFF);
        state->key1_high = (uint32_t)((key1 >> 32) & 0xFFFFFFFF);

        if(!flipper_format_read_string(ff, "Key_2", temp)) break;
        uint64_t key2 = 0;
        if(!parse_hex_key_str(furi_string_get_cstr(temp), &key2)) break;
        state->key2_low = (uint16_t)(key2 & 0xFFFF);

        state->cancel = 0;
        state->progress_current = 0;
        state->progress_total = 0;
        state->status = PSA_BF_STATUS_IDLE;
        state->on_done = NULL;
        state->on_done_ctx = NULL;
        ok = true;
    } while(false);
    furi_string_free(temp);
    return ok;
}

static bool psa_direct_xor_decrypt(SubGhzProtocolDecoderPSA* instance, uint8_t* buffer) {
    psa_calculate_checksum(buffer);
    uint8_t checksum = buffer[11];
    uint8_t key2_high = buffer[8];

    uint8_t validation_result = (checksum ^ key2_high) & 0xF0;
    FURI_LOG_D(
        TAG,
        "Direct XOR validation: checksum=0x%02X, key2_high=0x%02X, result=0x%02X",
        checksum,
        key2_high,
        validation_result);

    if(validation_result == 0) {
        buffer[13] = buffer[9] ^ buffer[8];
        psa_second_stage_xor_decrypt(buffer);
        psa_extract_fields_mode23(buffer, instance);
        FURI_LOG_D(TAG, "Direct XOR decryption completed");
        return true;
    }
    FURI_LOG_D(
        TAG,
        "Direct XOR validation FAILED: (checksum ^ key2_high) & 0xF0 = 0x%02X (expected 0x00)",
        validation_result);
    return false;
}

static void psa_handle_decoded_frame(SubGhzProtocolDecoderPSA* instance, uint8_t direct_xor_mode) {
    instance->key2_low = instance->decode_data_low;
    instance->key2_high = instance->decode_data_high;
    instance->validation_field = (uint16_t)(instance->decode_data_low & 0xFFFF);
    instance->decrypted_type = 0;
    instance->decrypted_button = 0;
    instance->decrypted_serial = 0;
    instance->decrypted_counter = 0;
    instance->decrypted_crc = 0;
    instance->decrypted_seed = 0;
    instance->decrypted = 0x00;
    instance->mode_serialize = psa_direct_xor_allowed_by_key2((uint8_t)(instance->key2_low >> 8)) ?
                                   direct_xor_mode :
                                   0x36;
    instance->status_flag = 0x80;

    FURI_LOG_I(
        TAG,
        "Signal decoded - Key1:%08lX%08lX Key2:%04X ValField:%04X",
        (unsigned long)instance->key1_high,
        (unsigned long)instance->key1_low,
        (unsigned int)(instance->key2_low & 0xFFFF),
        (unsigned int)instance->validation_field);

    uint8_t buffer[PSA_BUFFER_SIZE] = {0};
    psa_setup_byte_buffer(buffer, instance->key1_low, instance->key1_high, instance->key2_low);
    if(instance->mode_serialize != 0x36 && psa_direct_xor_decrypt(instance, buffer)) {
        instance->mode_serialize = 0x23;
        instance->decrypted = 0x50;
        FURI_LOG_I(
            TAG,
            "Direct XOR decryption SUCCESS - Type:0x23 Btn:%02X Ser:%06lX Cnt:%08lX",
            (unsigned int)instance->decrypted_button,
            (unsigned long)instance->decrypted_serial,
            (unsigned long)instance->decrypted_counter);
    } else {
        instance->decrypted = 0x00;
        instance->mode_serialize = 0x36;
        FURI_LOG_I(
            TAG,
            "Direct XOR decryption FAILED - marked for brute force (Type:0x36) - deferred to get_string");
    }
    if(instance->base.callback) {
        instance->base.callback(&instance->base, instance->base.context);
    }
    instance->decode_data_low = 0;
    instance->decode_data_high = 0;
    instance->decode_count_bit = 0;
}

static void psa_decrypt_router(SubGhzProtocolDecoderPSA* instance) {
    FURI_LOG_I(TAG, "=== DECRYPTION ROUTER CALLED ===");
    FURI_LOG_I(
        TAG,
        "Key1:%08lX%08lX Key2:%04X mode_serialize:0x%02X decrypted:0x%02X",
        (unsigned long)instance->key1_high,
        (unsigned long)instance->key1_low,
        (unsigned int)(instance->key2_low & 0xFFFF),
        (unsigned int)instance->mode_serialize,
        (unsigned int)instance->decrypted);

    uint8_t buffer[PSA_BUFFER_SIZE] = {0};

    psa_setup_byte_buffer(buffer, instance->key1_low, instance->key1_high, instance->key2_low);

    uint8_t key2_high_byte = buffer[8];
    uint8_t mode = instance->mode_serialize;

    if(mode == 1 || mode == 2) {
        if(!psa_direct_xor_allowed_by_key2(key2_high_byte)) {
            mode = 0x36;
            FURI_LOG_I(
                TAG,
                "Key2 high byte 0x%02X fails OG direct-XOR filter -> 0x36 (TEA BF)",
                key2_high_byte);
        } else if(psa_direct_xor_decrypt(instance, buffer)) {
            mode = 0x23;
            FURI_LOG_I(TAG, "Converted mode %d -> 0x23 (Direct XOR)", instance->mode_serialize);
        } else {
            mode = 0x36;
            FURI_LOG_I(
                TAG, "Converted mode %d -> 0x36 (TEA Brute Force)", instance->mode_serialize);
        }
    }

    if(mode == 0x23) {
        FURI_LOG_I(TAG, "Attempting Direct XOR decryption (Type 0x23)");
        if(psa_direct_xor_decrypt(instance, buffer)) {
            instance->mode_serialize = 0x23;
            instance->decrypted = 0x50;
            FURI_LOG_I(TAG, "Direct XOR decryption SUCCESS");
            return;
        }
        FURI_LOG_W(TAG, "Direct XOR decryption FAILED");
    } else if(mode == 0x36) {
        FURI_LOG_I(TAG, "TEA BF deferred - use Brute force in receiver info");
        return;
    } else {
        FURI_LOG_I(TAG, "Mode uninitialized (0x00) - trying direct XOR or BF");
        if(psa_direct_xor_allowed_by_key2(key2_high_byte) &&
           psa_direct_xor_decrypt(instance, buffer)) {
            instance->mode_serialize = 0x23;
            instance->decrypted = 0x50;
            FURI_LOG_I(TAG, "Direct XOR decryption SUCCESS");
            return;
        }

        FURI_LOG_I(TAG, "Direct XOR skipped or failed - trying brute force...");
        uint32_t w0, w1;
        psa_prepare_tea_data(buffer, &w0, &w1);

        if(psa_brute_force_decrypt_bf1(instance, buffer, w0, w1)) {
            instance->mode_serialize = 0x36;
            instance->decrypted = 0x50;
            FURI_LOG_I(TAG, "BF1 brute force SUCCESS");
            return;
        }
        if(psa_brute_force_decrypt_bf2(instance, buffer, w0, w1)) {
            instance->mode_serialize = 0x36;
            instance->decrypted = 0x50;
            FURI_LOG_I(TAG, "BF2 brute force SUCCESS");
            return;
        }
    }

    FURI_LOG_E(TAG, "=== ALL DECRYPTION ATTEMPTS FAILED ===");
    instance->decrypted = 0x00;
}

void* subghz_protocol_decoder_psa_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderPSA* instance = malloc(sizeof(SubGhzProtocolDecoderPSA));
    if(instance) {
        memset(instance, 0, sizeof(SubGhzProtocolDecoderPSA));
        instance->base.protocol = &psa_protocol;
        instance->manchester_state = ManchesterStateMid1;
    }
    return instance;
}

void subghz_protocol_decoder_psa_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;
    free(instance);
}

void subghz_protocol_decoder_psa_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;
    instance->state = 0;
    instance->status_flag = 0;
    instance->mode_serialize = 0;
    instance->key1_low = 0;
    instance->key1_high = 0;
    instance->key2_low = 0;
    instance->key2_high = 0;
    instance->decode_data_low = 0;
    instance->decode_data_high = 0;
    instance->decode_count_bit = 0;
    instance->pattern_counter = 0;
    instance->manchester_state = ManchesterStateMid1;
    instance->decrypted_button = 0;
    instance->decrypted_serial = 0;
    instance->decrypted_counter = 0;
    instance->decrypted_crc = 0;
    instance->decrypted_seed = 0;
    instance->decrypted_type = 0;
}

void subghz_protocol_decoder_psa_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;

    uint32_t tolerance;
    uint32_t new_state = instance->state;
    uint32_t prev_dur = instance->prev_duration;
    uint32_t te_short = subghz_protocol_psa_const.te_short;
    uint32_t te_long = subghz_protocol_psa_const.te_long;

    switch(instance->state) {
    case PSADecoderState0:
        if(!level) {
            return;
        }

        if(duration < te_short) {
            tolerance = te_short - duration;
            if(tolerance > PSA_TOLERANCE_99) {
                if(duration < PSA_TE_SHORT_125) {
                    tolerance = PSA_TE_SHORT_125 - duration;
                } else {
                    tolerance = duration - PSA_TE_SHORT_125;
                }
                if(tolerance > 40) {
                    return;
                }
                if(duration > 180) {
                    return;
                }
                new_state = PSADecoderState3;
            } else {
                new_state = PSADecoderState1;
            }
        } else {
            tolerance = duration - te_short;
            if(tolerance > PSA_TOLERANCE_99) {
                return;
            }
            new_state = PSADecoderState1;
        }

        instance->decode_data_low = 0;
        instance->decode_data_high = 0;
        instance->pattern_counter = 0;
        instance->decode_count_bit = 0;
        instance->mode_serialize = 0;
        instance->prev_duration = duration;
        instance->decrypted_type = 0;
        instance->decrypted_button = 0;
        instance->decrypted_serial = 0;
        instance->decrypted_counter = 0;
        instance->decrypted_crc = 0;
        instance->decrypted_seed = 0;
        instance->decrypted = 0x00;
        manchester_advance(
            instance->manchester_state, ManchesterEventReset, &instance->manchester_state, NULL);
        break;

    case PSADecoderState1:
        if(level) {
            return;
        }

        if(duration < te_short) {
            tolerance = te_short - duration;
            if(tolerance < PSA_TOLERANCE_100) {
                uint32_t prev_diff = psa_abs_diff(prev_dur, te_short);
                if(prev_diff <= PSA_TOLERANCE_99) {
                    instance->pattern_counter++;
                }
                instance->prev_duration = duration;
                return;
            }
        } else {
            tolerance = duration - te_short;
            if(tolerance < PSA_TOLERANCE_100) {
                uint32_t prev_diff = psa_abs_diff(prev_dur, te_short);
                if(prev_diff <= PSA_TOLERANCE_99) {
                    instance->pattern_counter++;
                }
                instance->prev_duration = duration;
                return;
            } else {
                uint32_t long_diff;
                if(duration < te_long) {
                    long_diff = te_long - duration;
                } else {
                    long_diff = duration - te_long;
                }
                if(long_diff < 100) {
                    if(instance->pattern_counter > PSA_PATTERN_THRESHOLD_1) {
                        FURI_LOG_I(
                            TAG,
                            "[State1->State2] Transition detected with pattern_cnt=%lu",
                            (unsigned long)instance->pattern_counter);
                        new_state = PSADecoderState2;
                        instance->decode_data_low = 0;
                        instance->decode_data_high = 0;
                        instance->decode_count_bit = 0;
                        manchester_advance(
                            instance->manchester_state,
                            ManchesterEventReset,
                            &instance->manchester_state,
                            NULL);
                        instance->state = new_state;
                    }
                    instance->pattern_counter = 0;
                    instance->prev_duration = duration;
                    return;
                }
            }
        }

        new_state = PSADecoderState0;
        instance->pattern_counter = 0;
        break;

    case PSADecoderState2:
        if(instance->decode_count_bit >= PSA_MAX_BITS) {
            new_state = PSADecoderState0;
            break;
        }

        if(level && instance->decode_count_bit == PSA_KEY2_BITS) {
            if(duration >= 800) {
                uint32_t end_diff;
                if(duration < PSA_TE_END_1000) {
                    end_diff = PSA_TE_END_1000 - duration;
                } else {
                    end_diff = duration - PSA_TE_END_1000;
                }
                if(end_diff <= 199) {
                    if(((instance->key1_high >> 16) & 0xF) != 0xA) {
                        instance->decode_data_low = 0;
                        instance->decode_data_high = 0;
                        instance->decode_count_bit = 0;
                        new_state = PSADecoderState0;
                        instance->state = new_state;
                        return;
                    }
                    psa_handle_decoded_frame(instance, 1);
                    new_state = PSADecoderState0;
                    instance->state = new_state;
                    return;
                }
            }
        }

        uint8_t manchester_input = 0;
        bool should_process = false;

        if(duration < te_short) {
            tolerance = te_short - duration;
            if(tolerance >= PSA_TOLERANCE_100) {
                return;
            }
            manchester_input = ((level ^ 1) & 0x7f) << 1;
            should_process = true;
        } else {
            tolerance = duration - te_short;
            if(tolerance < PSA_TOLERANCE_100) {
                manchester_input = ((level ^ 1) & 0x7f) << 1;
                should_process = true;
            } else if(duration < te_long) {
                uint32_t diff_from_250 = duration - te_short;
                uint32_t diff_from_500 = te_long - duration;

                if(diff_from_500 < 150 || diff_from_250 > diff_from_500) {
                    if(level == 0) {
                        manchester_input = 6;
                    } else {
                        manchester_input = 4;
                    }
                    should_process = true;
                } else if(diff_from_250 < 150) {
                    manchester_input = ((level ^ 1) & 0x7f) << 1;
                    should_process = true;
                } else {
                    if(duration > 10000) {
                        new_state = PSADecoderState0;
                        instance->pattern_counter = 0;
                        return;
                    }
                    if(duration >= 350 && duration <= 400) {
                        if(level == 0) {
                            manchester_input = 6;
                        } else {
                            manchester_input = 4;
                        }
                        should_process = true;
                    } else {
                        return;
                    }
                }
            } else {
                uint32_t long_diff = duration - te_long;
                if(long_diff < 100) {
                    if(level == 0) {
                        manchester_input = 6;
                    } else {
                        manchester_input = 4;
                    }
                    should_process = true;
                } else {
                    if(!level) {
                        if(duration > 10000) {
                            new_state = PSADecoderState0;
                            instance->pattern_counter = 0;
                            return;
                        }
                        return;
                    }
                    should_process = false;
                }
            }
        }

        if(should_process && instance->decode_count_bit < PSA_KEY2_BITS) {
            bool decoded_bit = false;

            if(manchester_advance(
                   instance->manchester_state,
                   (ManchesterEvent)manchester_input,
                   &instance->manchester_state,
                   &decoded_bit)) {
                uint32_t carry = (instance->decode_data_low >> 31) & 1;
                instance->decode_data_low = (instance->decode_data_low << 1) |
                                            (decoded_bit ? 1 : 0);
                instance->decode_data_high = (instance->decode_data_high << 1) | carry;
                instance->decode_count_bit++;

                if(instance->decode_count_bit == PSA_KEY1_BITS) {
                    instance->key1_low = instance->decode_data_low;
                    instance->key1_high = instance->decode_data_high;
                    instance->decode_data_low = 0;
                    instance->decode_data_high = 0;
                }
            }
        }

        if(!level) {
            return;
        }

        if(!should_process) {
            uint32_t end_diff;
            if(duration < PSA_TE_END_1000) {
                end_diff = PSA_TE_END_1000 - duration;
            } else {
                end_diff = duration - PSA_TE_END_1000;
            }
            if(end_diff <= 199) {
                if(instance->decode_count_bit != PSA_KEY2_BITS) {
                    return;
                }

                if(((instance->key1_high >> 16) & 0xF) == 0xA) {
                    psa_handle_decoded_frame(instance, 1);
                    new_state = PSADecoderState0;
                    instance->state = new_state;
                    return;
                } else {
                    return;
                }
            } else {
                return;
            }
        }
        break;

    case PSADecoderState3:
        if(duration >= 250) {
            if(duration >= PSA_TE_LONG_250 && duration < PSA_TE_LONG_300) {
                if(instance->pattern_counter > PSA_PATTERN_THRESHOLD_2) {
                    FURI_LOG_I(
                        TAG,
                        "[State3->State4] Transition detected with pattern_cnt=%lu",
                        (unsigned long)instance->pattern_counter);
                    new_state = PSADecoderState4;
                    instance->decode_data_low = 0;
                    instance->decode_data_high = 0;
                    instance->decode_count_bit = 0;
                    manchester_advance(
                        instance->manchester_state,
                        ManchesterEventReset,
                        &instance->manchester_state,
                        NULL);
                    instance->state = new_state;
                    instance->pattern_counter = 0;
                    instance->prev_duration = duration;
                    return;
                }
            }
            new_state = PSADecoderState0;
            instance->pattern_counter = 0;
            break;
        }

        if(duration < PSA_TE_SHORT_125) {
            tolerance = PSA_TE_SHORT_125 - duration;
        } else {
            tolerance = duration - PSA_TE_SHORT_125;
        }

        if(tolerance < PSA_TOLERANCE_50) {
            uint32_t prev_diff = psa_abs_diff(prev_dur, PSA_TE_SHORT_125);
            if(prev_diff <= PSA_TOLERANCE_49) {
                instance->pattern_counter++;
            } else {
                instance->pattern_counter = 0;
            }
            instance->prev_duration = duration;
            return;
        }

        new_state = PSADecoderState0;
        instance->pattern_counter = 0;
        break;

    case PSADecoderState4:
        if(instance->decode_count_bit >= PSA_MAX_BITS) {
            new_state = PSADecoderState0;
            break;
        }

        if(!level) {
            uint8_t manchester_input;
            bool decoded_bit = false;

            if(duration < PSA_TE_SHORT_125) {
                tolerance = PSA_TE_SHORT_125 - duration;
                if(tolerance > PSA_TOLERANCE_49) {
                    return;
                }
                manchester_input = ((level ^ 1) & 0x7f) << 1;
            } else {
                tolerance = duration - PSA_TE_SHORT_125;
                if(tolerance < PSA_TOLERANCE_50) {
                    manchester_input = ((level ^ 1) & 0x7f) << 1;
                } else if(duration >= PSA_TE_LONG_250 && duration < PSA_TE_LONG_300) {
                    if(level == 0) {
                        manchester_input = 6;
                    } else {
                        manchester_input = 4;
                    }
                } else {
                    return;
                }
            }

            if(manchester_advance(
                   instance->manchester_state,
                   (ManchesterEvent)manchester_input,
                   &instance->manchester_state,
                   &decoded_bit)) {
                uint32_t carry = (instance->decode_data_low >> 31) & 1;
                instance->decode_data_low = (instance->decode_data_low << 1) |
                                            (decoded_bit ? 1 : 0);
                instance->decode_data_high = (instance->decode_data_high << 1) | carry;
                instance->decode_count_bit++;

                if(instance->decode_count_bit == PSA_KEY1_BITS) {
                    instance->key1_low = instance->decode_data_low;
                    instance->key1_high = instance->decode_data_high;
                    instance->decode_data_low = 0;
                    instance->decode_data_high = 0;
                }
            }
        } else if(level) {
            uint32_t end_diff;
            if(duration < PSA_TE_END_500) {
                end_diff = PSA_TE_END_500 - duration;
            } else {
                end_diff = duration - PSA_TE_END_500;
            }
            if(end_diff <= 99) {
                if(instance->decode_count_bit != PSA_KEY2_BITS) {
                    return;
                }

                if(((instance->key1_high >> 16) & 0xF) != 0xA) {
                    instance->decode_data_low = 0;
                    instance->decode_data_high = 0;
                    instance->decode_count_bit = 0;
                    new_state = PSADecoderState0;
                    instance->state = new_state;
                    return;
                }
                psa_handle_decoded_frame(instance, 2);
                new_state = PSADecoderState0;
                instance->state = new_state;
                return;
            } else {
                return;
            }
        }
        break;
    }

    instance->state = new_state;
    instance->prev_duration = duration;
}

uint8_t subghz_protocol_decoder_psa_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;
    uint64_t combined_data = ((uint64_t)instance->key1_high << 32) | instance->key1_low;
    SubGhzBlockDecoder decoder = {.decode_data = combined_data, .decode_count_bit = 64};
    return subghz_protocol_blocks_get_hash_data(&decoder, 16);
}

SubGhzProtocolStatus subghz_protocol_decoder_psa_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    do {
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Preset", furi_string_get_cstr(preset->name)))
            break;

        if(!flipper_format_write_string_cstr(
               flipper_format, "Protocol", instance->base.protocol->name))
            break;

        uint32_t bits = 128;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &bits, 1)) break;

        char key1_str[20];
        uint64_t key1 = ((uint64_t)instance->key1_high << 32) | instance->key1_low;
        snprintf(key1_str, sizeof(key1_str), "%016llX", key1);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key1_str)) break;

        char key2_str[20];
        uint64_t key2 = ((uint64_t)instance->key2_high << 32) | instance->key2_low;
        snprintf(key2_str, sizeof(key2_str), "%016llX", key2);
        if(!flipper_format_write_string_cstr(flipper_format, "Key_2", key2_str)) break;

        uint32_t val_field = instance->validation_field;
        flipper_format_write_uint32(flipper_format, "ValidationField", &val_field, 1);

        if(instance->decrypted == 0x50 && instance->decrypted_type != 0) {
            flipper_format_write_uint32(flipper_format, "Serial", &instance->decrypted_serial, 1);

            uint32_t btn_temp = instance->decrypted_button;
            flipper_format_write_uint32(flipper_format, "Btn", &btn_temp, 1);

            flipper_format_write_uint32(flipper_format, "Cnt", &instance->decrypted_counter, 1);

            uint32_t crc_temp = instance->decrypted_crc;
            flipper_format_write_uint32(flipper_format, "CRC", &crc_temp, 1);

            uint32_t type_temp = instance->decrypted_type;
            flipper_format_write_uint32(flipper_format, "Type", &type_temp, 1);

            flipper_format_write_uint32(flipper_format, "Seed", &instance->decrypted_seed, 1);
        }

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_psa_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;
    FuriString* temp_str = furi_string_alloc();

    do {
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) break;
        uint64_t key1 = 0;
        if(!parse_hex_key_str(furi_string_get_cstr(temp_str), &key1)) break;
        instance->key1_low = (uint32_t)(key1 & 0xFFFFFFFF);
        instance->key1_high = (uint32_t)((key1 >> 32) & 0xFFFFFFFF);

        if(!flipper_format_read_string(flipper_format, "Key_2", temp_str)) break;
        uint64_t key2 = 0;
        if(!parse_hex_key_str(furi_string_get_cstr(temp_str), &key2)) break;
        instance->key2_low = (uint32_t)(key2 & 0xFFFFFFFF);
        instance->key2_high = (uint32_t)((key2 >> 32) & 0xFFFFFFFF);

        instance->status_flag = 0x80;

        psa_decrypt_router(instance);

        ret = SubGhzProtocolStatusOk;
    } while(false);

    furi_string_free(temp_str);
    return ret;
}

void subghz_protocol_decoder_psa_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderPSA* instance = context;

    if(instance->status_flag == 0x80 && (instance->key1_low != 0 || instance->key1_high != 0) &&
       instance->decrypted_type == 0) {
        FURI_LOG_I(TAG, "get_string: Calling decryption router (decrypted_type=0)");
        psa_decrypt_router(instance);
    } else {
        FURI_LOG_D(
            TAG,
            "get_string: Skipping router - status_flag=0x%08lX decrypted_type=0x%02X",
            (unsigned long)instance->status_flag,
            (unsigned int)instance->decrypted_type);
    }

    uint16_t key2_value = (uint16_t)(instance->key2_low & 0xFFFF);

    if(instance->decrypted == 0x50 && instance->decrypted_type != 0) {
        if(instance->decrypted_type == 0x23) {
            furi_string_printf(
                output,
                "%s %dbit\r\n"
                "Key1:%08lX%08lX\r\n"
                "Key2:%04X\r\n"
                "Btn:%01X\r\n"
                "Ser:%06lX\r\n"
                "Cnt:%04lX\r\n"
                "CRC:%02X\r\n"
                "Type:%02X\r\n"
                "Sd:%06lX",
                instance->base.protocol->name,
                128,
                instance->key1_high,
                instance->key1_low,
                key2_value,
                instance->decrypted_button,
                instance->decrypted_serial,
                (uint32_t)instance->decrypted_counter,
                instance->decrypted_crc,
                instance->decrypted_type,
                instance->decrypted_seed);
        } else if(instance->decrypted_type == 0x36) {
            furi_string_printf(
                output,
                "%s %dbit\r\n"
                "Key1:%08lX%08lX\r\n"
                "Key2:%04X\r\n"
                "Btn:%02X\r\n"
                "Ser:%06lX\r\n"
                "Cnt:%lX\r\n"
                "CRC:%02X\r\n"
                "Type:%02X\r\n"
                "Sd:%06lX",
                instance->base.protocol->name,
                128,
                instance->key1_high,
                instance->key1_low,
                key2_value,
                instance->decrypted_button,
                instance->decrypted_serial,
                instance->decrypted_counter,
                instance->decrypted_crc,
                instance->decrypted_type,
                instance->decrypted_seed);
        } else {
            furi_string_printf(
                output,
                "%s %dbit\r\n"
                "Key1:%08lX%08lX\r\n"
                "Key2:%X",
                instance->base.protocol->name,
                128,
                instance->key1_high,
                instance->key1_low,
                key2_value);
        }
    } else {
        furi_string_printf(
            output,
            "%s %dbit\r\n"
            "Key1:%08lX%08lX\r\n"
            "Key2:%X",
            instance->base.protocol->name,
            128,
            instance->key1_high,
            instance->key1_low,
            key2_value);
    }
}
