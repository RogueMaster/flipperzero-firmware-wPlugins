#include "ford_v0.h"
#include "../protopirate_app_i.h"

#define TAG "FordProtocolV0"

// =============================================================================
// PROTOCOL CONSTANTS
// =============================================================================

// Uncomment to enable bit-level debug logging (WARNING: 80 log calls per signal)
// #define FORD_V0_DEBUG_BITS

static const SubGhzBlockConst subghz_protocol_ford_v0_const = {
    .te_short = 250,
    .te_long = 500,
    .te_delta = 100,
    .min_count_bit_for_found = 64,
};

#define FORD_V0_PREAMBLE_PAIRS 4
#define FORD_V0_GAP_US         3500
#define FORD_V0_TOTAL_BURSTS   6

// =============================================================================
// CRC MATRIX
// Ford V0 uses matrix multiplication in GF(2) for CRC calculation
// =============================================================================

static const uint8_t ford_v0_crc_matrix[64] = {
    0xDA, 0xB5, 0x55, 0x6A, 0xAA, 0xAA, 0xAA, 0xD5, 0xB6, 0x6C, 0xCC, 0xD9, 0x99, 0x99, 0x99, 0xB3,
    0x71, 0xE3, 0xC3, 0xC7, 0x87, 0x87, 0x87, 0x8F, 0x0F, 0xE0, 0x3F, 0xC0, 0x7F, 0x80, 0x7F, 0x80,
    0x00, 0x1F, 0xFF, 0xC0, 0x00, 0x7F, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x23, 0x12, 0x94, 0x84, 0x35, 0xF4, 0x55, 0x84,
};

// =============================================================================
// STRUCT DEFINITIONS
// =============================================================================

typedef struct SubGhzProtocolDecoderFordV0 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;

    ManchesterState manchester_state;

    uint64_t data_low;
    uint64_t data_high;
    uint8_t bit_count;

    uint16_t header_count;

    uint64_t key1;
    uint16_t key2;
    uint32_t serial;
    uint8_t button;
    uint32_t count;
} SubGhzProtocolDecoderFordV0;
#ifdef ENABLE_EMULATE_FEATURE
typedef struct SubGhzProtocolEncoderFordV0 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint64_t key1;
    uint16_t key2;
    uint32_t serial;
    uint8_t button;
    uint32_t count;
    uint8_t checksum;
} SubGhzProtocolEncoderFordV0;
#endif
typedef enum {
    FordV0DecoderStepReset = 0,
    FordV0DecoderStepPreamble,
    FordV0DecoderStepPreambleCheck,
    FordV0DecoderStepGap,
    FordV0DecoderStepData,
} FordV0DecoderStep;

// =============================================================================
// FUNCTION PROTOTYPES
// =============================================================================

static void ford_v0_add_bit(SubGhzProtocolDecoderFordV0* instance, bool bit);
static void decode_ford_v0(
    uint64_t key1,
    uint16_t key2,
    uint32_t* serial,
    uint8_t* button,
    uint32_t* count);
#ifdef ENABLE_EMULATE_FEATURE
static void encode_ford_v0(
    uint8_t header_byte,
    uint32_t serial,
    uint8_t button,
    uint32_t count,
    uint8_t checksum,
    uint64_t* key1);
#endif
static bool ford_v0_process_data(SubGhzProtocolDecoderFordV0* instance);

// =============================================================================
// PROTOCOL INTERFACE DEFINITIONS
// =============================================================================

const SubGhzProtocolDecoder subghz_protocol_ford_v0_decoder = {
    .alloc = subghz_protocol_decoder_ford_v0_alloc,
    .free = subghz_protocol_decoder_ford_v0_free,
    .feed = subghz_protocol_decoder_ford_v0_feed,
    .reset = subghz_protocol_decoder_ford_v0_reset,
    .get_hash_data = subghz_protocol_decoder_ford_v0_get_hash_data,
    .serialize = subghz_protocol_decoder_ford_v0_serialize,
    .deserialize = subghz_protocol_decoder_ford_v0_deserialize,
    .get_string = subghz_protocol_decoder_ford_v0_get_string,
};

#ifdef ENABLE_EMULATE_FEATURE
const SubGhzProtocolEncoder subghz_protocol_ford_v0_encoder = {
    .alloc = subghz_protocol_encoder_ford_v0_alloc,
    .free = subghz_protocol_encoder_ford_v0_free,
    .deserialize = subghz_protocol_encoder_ford_v0_deserialize,
    .stop = subghz_protocol_encoder_ford_v0_stop,
    .yield = subghz_protocol_encoder_ford_v0_yield,
};
#else
const SubGhzProtocolEncoder subghz_protocol_ford_v0_encoder = {
    .alloc = NULL,
    .free = NULL,
    .deserialize = NULL,
    .stop = NULL,
    .yield = NULL,
};
#endif

const SubGhzProtocol ford_protocol_v0 = {
    .name = FORD_PROTOCOL_V0_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_Decodable |
            SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save | SubGhzProtocolFlag_Send,
    .decoder = &subghz_protocol_ford_v0_decoder,
    .encoder = &subghz_protocol_ford_v0_encoder,
};

// =============================================================================
// CHECKSUM CALCULATION
// =============================================================================
#ifdef ENABLE_EMULATE_FEATURE
static uint8_t ford_v0_calculate_checksum(uint32_t serial, uint32_t count, uint8_t button) {
    return (uint8_t)((((count >> 24) & 0xFF) + ((count >> 16) & 0xFF) + ((count >> 8) & 0xFF) +
                      (count & 0xFF) + ((serial >> 24) & 0xFF) + ((serial >> 16) & 0xFF) +
                      ((serial >> 8) & 0xFF) + (serial & 0xFF) + (button << 3)) &
                     0xFF);
}
#endif
// =============================================================================
// CRC FUNCTIONS
// =============================================================================

static uint8_t ford_v0_popcount8(uint8_t x) {
    uint8_t count = 0;
    while(x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

static uint8_t ford_v0_calculate_crc(uint8_t* buf) {
    uint8_t crc = 0;

    for(int row = 0; row < 8; row++) {
        uint8_t xor_sum = 0;
        for(int col = 0; col < 8; col++) {
            xor_sum ^= (ford_v0_crc_matrix[row * 8 + col] & buf[col + 1]);
        }
        uint8_t parity = ford_v0_popcount8(xor_sum) & 1;
        if(parity) {
            crc |= (1 << row);
        }
    }

    return crc;
}
#ifdef ENABLE_EMULATE_FEATURE
static uint8_t ford_v0_calculate_crc_for_tx(uint64_t key1, uint8_t checksum) {
    uint8_t buf[16] = {0};

    for(int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(key1 >> (56 - i * 8));
    }

    buf[8] = checksum;

    uint8_t crc = ford_v0_calculate_crc(buf);
    return crc ^ 0x80;
}
#endif
static bool ford_v0_verify_crc(uint64_t key1, uint16_t key2) {
    uint8_t buf[16] = {0};

    for(int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(key1 >> (56 - i * 8));
    }

    buf[8] = (uint8_t)(key2 >> 8);

    uint8_t calculated_crc = ford_v0_calculate_crc(buf);
    uint8_t received_crc = (uint8_t)(key2 & 0xFF) ^ 0x80;

    return ((calculated_crc & 0x7F) == (received_crc & 0x7F));
}

// =============================================================================
// DECODE FUNCTION
// =============================================================================

static void decode_ford_v0(
    uint64_t key1,
    uint16_t key2,
    uint32_t* serial,
    uint8_t* button,
    uint32_t* count) {
    uint8_t buf[13] = {0};

    for(int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(key1 >> (56 - i * 8));
    }

    buf[8] = (uint8_t)(key2 >> 8);
    buf[9] = (uint8_t)(key2 & 0xFF);

    uint8_t tmp = buf[8];
    uint8_t parity = 0;
    uint8_t parity_any = (tmp != 0);
    while(tmp) {
        parity ^= (tmp & 1);
        tmp >>= 1;
    }
    buf[11] = parity_any ? parity : 0;

    uint8_t xor_byte;
    uint8_t limit;
    if(buf[11]) {
        xor_byte = buf[7];
        limit = 7;
    } else {
        xor_byte = buf[6];
        limit = 6;
    }

    for(int idx = 1; idx < limit; ++idx) {
        buf[idx] ^= xor_byte;
    }

    if(buf[11] == 0) {
        buf[7] ^= xor_byte;
    }

    uint8_t orig_b7 = buf[7];
    buf[7] = (orig_b7 & 0xAA) | (buf[6] & 0x55);
    uint8_t mixed = (buf[6] & 0xAA) | (orig_b7 & 0x55);
    buf[12] = mixed;
    buf[6] = mixed;

    uint32_t serial_le = ((uint32_t)buf[1]) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[3] << 16) |
                         ((uint32_t)buf[4] << 24);

    *serial = ((serial_le & 0xFF) << 24) | (((serial_le >> 8) & 0xFF) << 16) |
              (((serial_le >> 16) & 0xFF) << 8) | ((serial_le >> 24) & 0xFF);

    *button = (buf[5] >> 3) & 0x0F;

    *count = ((buf[5] & 0x07) << 16) | (buf[6] << 8) | buf[7];
}

// =============================================================================
// ENCODE FUNCTION
// =============================================================================
#ifdef ENABLE_EMULATE_FEATURE
static void encode_ford_v0(
    uint8_t header_byte,
    uint32_t serial,
    uint8_t button,
    uint32_t count,
    uint8_t checksum,
    uint64_t* key1) {
    if(!key1) {
        FURI_LOG_E(TAG, "encode_ford_v0: NULL key1 pointer");
        return;
    }

    uint8_t buf[8] = {0};

    buf[0] = header_byte;

    buf[1] = (serial >> 24) & 0xFF;
    buf[2] = (serial >> 16) & 0xFF;
    buf[3] = (serial >> 8) & 0xFF;
    buf[4] = serial & 0xFF;

    buf[5] = ((button & 0x0F) << 3) | ((count >> 16) & 0x0F);

    uint8_t count_mid = (count >> 8) & 0xFF;
    uint8_t count_low = count & 0xFF;

    uint8_t post_xor_6 = (count_mid & 0xAA) | (count_low & 0x55);
    uint8_t post_xor_7 = (count_low & 0xAA) | (count_mid & 0x55);

    uint8_t parity = 0;
    uint8_t tmp = checksum;
    while(tmp) {
        parity ^= (tmp & 1);
        tmp >>= 1;
    }
    bool parity_bit = (checksum != 0) ? (parity != 0) : false;

    if(parity_bit) {
        uint8_t xor_byte = post_xor_7;
        buf[1] ^= xor_byte;
        buf[2] ^= xor_byte;
        buf[3] ^= xor_byte;
        buf[4] ^= xor_byte;
        buf[5] ^= xor_byte;
        buf[6] = post_xor_6 ^ xor_byte;
        buf[7] = post_xor_7;
    } else {
        uint8_t xor_byte = post_xor_6;
        buf[1] ^= xor_byte;
        buf[2] ^= xor_byte;
        buf[3] ^= xor_byte;
        buf[4] ^= xor_byte;
        buf[5] ^= xor_byte;
        buf[6] = post_xor_6;
        buf[7] = post_xor_7 ^ xor_byte;
    }

    *key1 = 0;
    for(int i = 0; i < 8; i++) {
        *key1 = (*key1 << 8) | buf[i];
    }

    FURI_LOG_I(
        TAG,
        "Encode: Sn=%08lX Btn=%d Cnt=%05lX Checksum=%02X",
        (unsigned long)serial,
        button,
        (unsigned long)count,
        checksum);
    FURI_LOG_I(
        TAG,
        "Encode key1: %08lX%08lX",
        (unsigned long)(*key1 >> 32),
        (unsigned long)(*key1 & 0xFFFFFFFF));
}

// =============================================================================
// ENCODER IMPLEMENTATION
// =============================================================================

void* subghz_protocol_encoder_ford_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderFordV0* instance = malloc(sizeof(SubGhzProtocolEncoderFordV0));

    instance->base.protocol = &ford_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->encoder.repeat = 10;
    instance->encoder.size_upload = 1024;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.is_running = false;
    instance->encoder.front = 0;

    instance->key1 = 0;
    instance->key2 = 0;
    instance->serial = 0;
    instance->button = 0;
    instance->count = 0;
    instance->checksum = 0;

    FURI_LOG_I(TAG, "Encoder allocated");
    return instance;
}

void subghz_protocol_encoder_ford_v0_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFordV0* instance = context;
    if(instance->encoder.upload) {
        free(instance->encoder.upload);
    }
    free(instance);
}

static void subghz_protocol_encoder_ford_v0_get_upload(SubGhzProtocolEncoderFordV0* instance) {
    furi_check(instance);
    size_t index = 0;

    uint64_t tx_key1 = ~instance->key1;
    uint16_t tx_key2 = ~instance->key2;

    FURI_LOG_I(
        TAG,
        "Building upload: key1=%08lX%08lX key2=%04X",
        (unsigned long)(instance->key1 >> 32),
        (unsigned long)(instance->key1 & 0xFFFFFFFF),
        instance->key2);

    uint32_t te_short = subghz_protocol_ford_v0_const.te_short;
    uint32_t te_long = subghz_protocol_ford_v0_const.te_long;

#define ADD_LEVEL(lvl, dur)                                                                       \
    do {                                                                                          \
        if(index > 0 && level_duration_get_level(instance->encoder.upload[index - 1]) == (lvl)) { \
            uint32_t prev = level_duration_get_duration(instance->encoder.upload[index - 1]);     \
            instance->encoder.upload[index - 1] = level_duration_make((lvl), prev + (dur));       \
        } else {                                                                                  \
            instance->encoder.upload[index++] = level_duration_make((lvl), (dur));                \
        }                                                                                         \
    } while(0)

    for(uint8_t burst = 0; burst < FORD_V0_TOTAL_BURSTS; burst++) {
        ADD_LEVEL(true, te_short);
        ADD_LEVEL(false, te_long);

        for(int i = 0; i < FORD_V0_PREAMBLE_PAIRS; i++) {
            ADD_LEVEL(true, te_long);
            ADD_LEVEL(false, te_long);
        }

        ADD_LEVEL(true, te_short);
        ADD_LEVEL(false, FORD_V0_GAP_US);

        bool first_bit = (tx_key1 >> 62) & 1;
        if(first_bit) {
            ADD_LEVEL(true, te_long);
        } else {
            ADD_LEVEL(true, te_short);
            ADD_LEVEL(false, te_long);
        }

        bool prev_bit = first_bit;

        for(int bit = 61; bit >= 0; bit--) {
            bool curr_bit = (tx_key1 >> bit) & 1;

            if(!prev_bit && !curr_bit) {
                ADD_LEVEL(true, te_short);
                ADD_LEVEL(false, te_short);
            } else if(!prev_bit && curr_bit) {
                ADD_LEVEL(true, te_long);
            } else if(prev_bit && !curr_bit) {
                ADD_LEVEL(false, te_long);
            } else {
                ADD_LEVEL(false, te_short);
                ADD_LEVEL(true, te_short);
            }

            prev_bit = curr_bit;
        }

        for(int bit = 15; bit >= 0; bit--) {
            bool curr_bit = (tx_key2 >> bit) & 1;

            if(!prev_bit && !curr_bit) {
                ADD_LEVEL(true, te_short);
                ADD_LEVEL(false, te_short);
            } else if(!prev_bit && curr_bit) {
                ADD_LEVEL(true, te_long);
            } else if(prev_bit && !curr_bit) {
                ADD_LEVEL(false, te_long);
            } else {
                ADD_LEVEL(false, te_short);
                ADD_LEVEL(true, te_short);
            }

            prev_bit = curr_bit;
        }

        if(burst < FORD_V0_TOTAL_BURSTS - 1) {
            ADD_LEVEL(false, te_long * 100);
        }
    }

#undef ADD_LEVEL

    instance->encoder.size_upload = index;
    instance->encoder.front = 0;

    FURI_LOG_I(TAG, "Upload built: %d bursts, size=%zu", FORD_V0_TOTAL_BURSTS, index);
}

SubGhzProtocolStatus
    subghz_protocol_encoder_ford_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderFordV0* instance = context;
    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    FURI_LOG_I(TAG, "Encoder deserialize started");

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    instance->encoder.repeat = 10;

    do {
        FuriString* temp_str = furi_string_alloc();
        if(!temp_str) {
            FURI_LOG_E(TAG, "Failed to allocate temp string");
            break;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol field");
            furi_string_free(temp_str);
            break;
        }

        if(!furi_string_equal(temp_str, instance->base.protocol->name)) {
            FURI_LOG_E(TAG, "Protocol mismatch: %s", furi_string_get_cstr(temp_str));
            furi_string_free(temp_str);
            break;
        }

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_string(flipper_format, "Key", temp_str)) {
            FURI_LOG_E(TAG, "Missing Key field");
            furi_string_free(temp_str);
            break;
        }

        const char* key_str = furi_string_get_cstr(temp_str);
        uint64_t original_key1 = 0;
        size_t str_len = strlen(key_str);
        size_t hex_pos = 0;

        for(size_t i = 0; i < str_len && hex_pos < 16; i++) {
            char c = key_str[i];
            if(c == ' ') continue;

            uint8_t nibble;
            if(c >= '0' && c <= '9')
                nibble = c - '0';
            else if(c >= 'A' && c <= 'F')
                nibble = c - 'A' + 10;
            else if(c >= 'a' && c <= 'f')
                nibble = c - 'a' + 10;
            else
                break;

            original_key1 = (original_key1 << 4) | nibble;
            hex_pos++;
        }
        furi_string_free(temp_str);

        uint8_t header_byte = (uint8_t)(original_key1 >> 56);
        FURI_LOG_I(
            TAG,
            "Original key1: %08lX%08lX, header=0x%02X",
            (unsigned long)(original_key1 >> 32),
            (unsigned long)(original_key1 & 0xFFFFFFFF),
            header_byte);

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1)) {
            FURI_LOG_E(TAG, "Missing Serial field");
            break;
        }
        instance->generic.serial = instance->serial;
        FURI_LOG_I(TAG, "Serial: 0x%08lX", (unsigned long)instance->serial);

        flipper_format_rewind(flipper_format);
        uint32_t btn_temp = 0;
        if(!flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1)) {
            FURI_LOG_E(TAG, "Missing Btn field");
            break;
        }
        instance->button = (uint8_t)btn_temp;
        instance->generic.btn = instance->button;
        FURI_LOG_I(TAG, "Button: 0x%02X", instance->button);

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Cnt", &instance->count, 1)) {
            FURI_LOG_E(TAG, "Missing Cnt field");
            break;
        }
        instance->generic.cnt = instance->count;
        FURI_LOG_I(TAG, "Counter: 0x%05lX", (unsigned long)instance->count);

        // Calculate Checksum from counter and button.
        instance->checksum =
            ford_v0_calculate_checksum(instance->serial, instance->count, instance->button);
        FURI_LOG_I(
            TAG,
            "Calculated Checksum: 0x%02X (from Cnt=0x%05lX, Btn=0x%02X)",
            instance->checksum,
            (unsigned long)instance->count,
            instance->button);

        encode_ford_v0(
            header_byte,
            instance->serial,
            instance->button,
            instance->count,
            instance->checksum,
            &instance->key1);

        instance->generic.data = instance->key1;
        instance->generic.data_count_bit = 64;

        uint8_t calculated_crc = ford_v0_calculate_crc_for_tx(instance->key1, instance->checksum);
        instance->key2 = ((uint16_t)instance->checksum << 8) | calculated_crc;

        FURI_LOG_I(
            TAG,
            "Final key2: 0x%04X (Checksum=0x%02X, CRC=0x%02X)",
            instance->key2,
            instance->checksum,
            calculated_crc);

        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(
               flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1)) {
            instance->encoder.repeat = 10;
        }

        subghz_protocol_encoder_ford_v0_get_upload(instance);

        if(instance->encoder.size_upload == 0) {
            FURI_LOG_E(TAG, "Upload build failed");
            break;
        }

        //Update the PSF file, since we have overwritten the COUNTER and BUTTON
        //This makes the file's nummers wrong, and fails tests. It wasnt causing a TX bug, but manual tests failed.
        flipper_format_rewind(flipper_format);
        uint32_t temp = calculated_crc;
        flipper_format_insert_or_update_uint32(flipper_format, "CRC", &temp, 1);
        temp = instance->checksum;
        flipper_format_insert_or_update_uint32(flipper_format, "Checksum", &temp, 1);

        instance->encoder.is_running = true;

        FURI_LOG_I(
            TAG,
            "Encoder ready: size=%zu repeat=%u",
            instance->encoder.size_upload,
            instance->encoder.repeat);

        ret = SubGhzProtocolStatusOk;
    } while(false);

    FURI_LOG_I(TAG, "Encoder deserialize finished, status=%d", ret);
    return ret;
}

void subghz_protocol_encoder_ford_v0_stop(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFordV0* instance = context;
    instance->encoder.is_running = false;
}

LevelDuration subghz_protocol_encoder_ford_v0_yield(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderFordV0* instance = context;

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
#endif
// =============================================================================
// DECODER IMPLEMENTATION
// =============================================================================

static void ford_v0_add_bit(SubGhzProtocolDecoderFordV0* instance, bool bit) {
#ifdef FORD_V0_DEBUG_BITS
    FURI_LOG_D(TAG, "Bit %d: %d", instance->bit_count, bit);
#endif

    uint32_t low = (uint32_t)instance->data_low;
    instance->data_low = (instance->data_low << 1) | (bit ? 1 : 0);
    instance->data_high = (instance->data_high << 1) | ((low >> 31) & 1);
    instance->bit_count++;
}

static bool ford_v0_process_data(SubGhzProtocolDecoderFordV0* instance) {
    if(instance->bit_count == 64) {
        uint64_t combined = ((uint64_t)instance->data_high << 32) | instance->data_low;
        instance->key1 = ~combined;
        instance->data_low = 0;
        instance->data_high = 0;
        return false;
    }

    if(instance->bit_count == 80) {
        uint16_t key2_raw = (uint16_t)(instance->data_low & 0xFFFF);
        uint16_t key2 = ~key2_raw;

        decode_ford_v0(
            instance->key1, key2, &instance->serial, &instance->button, &instance->count);

        instance->key2 = key2;
        return true;
    }

    return false;
}

void* subghz_protocol_decoder_ford_v0_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderFordV0* instance = malloc(sizeof(SubGhzProtocolDecoderFordV0));
    instance->base.protocol = &ford_protocol_v0;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void subghz_protocol_decoder_ford_v0_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;
    free(instance);
}

void subghz_protocol_decoder_ford_v0_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;

    instance->decoder.parser_step = FordV0DecoderStepReset;
    instance->decoder.te_last = 0;
    instance->manchester_state = ManchesterStateMid1;
    instance->data_low = 0;
    instance->data_high = 0;
    instance->bit_count = 0;
    instance->header_count = 0;
    instance->key1 = 0;
    instance->key2 = 0;
    instance->serial = 0;
    instance->button = 0;
    instance->count = 0;
}

void subghz_protocol_decoder_ford_v0_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;

    uint32_t te_short = subghz_protocol_ford_v0_const.te_short;
    uint32_t te_long = subghz_protocol_ford_v0_const.te_long;
    uint32_t te_delta = subghz_protocol_ford_v0_const.te_delta;
    uint32_t gap_threshold = 3500;

    switch(instance->decoder.parser_step) {
    case FordV0DecoderStepReset:
        if(level && (DURATION_DIFF(duration, te_short) < te_delta)) {
            instance->data_low = 0;
            instance->data_high = 0;
            instance->decoder.parser_step = FordV0DecoderStepPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 0;
            instance->bit_count = 0;
            manchester_advance(
                instance->manchester_state,
                ManchesterEventReset,
                &instance->manchester_state,
                NULL);
        }
        break;

    case FordV0DecoderStepPreamble:
        if(!level) {
            if(DURATION_DIFF(duration, te_long) < te_delta) {
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = FordV0DecoderStepPreambleCheck;
            } else {
                instance->decoder.parser_step = FordV0DecoderStepReset;
            }
        }
        break;

    case FordV0DecoderStepPreambleCheck:
        if(level) {
            if(DURATION_DIFF(duration, te_long) < te_delta) {
                instance->header_count++;
                instance->decoder.te_last = duration;
                instance->decoder.parser_step = FordV0DecoderStepPreamble;
            } else if(DURATION_DIFF(duration, te_short) < te_delta) {
                instance->decoder.parser_step = FordV0DecoderStepGap;
            } else {
                instance->decoder.parser_step = FordV0DecoderStepReset;
            }
        }
        break;

    case FordV0DecoderStepGap:
        if(!level && (DURATION_DIFF(duration, gap_threshold) < 250)) {
            instance->data_low = 1;
            instance->data_high = 0;
            instance->bit_count = 1;
            instance->decoder.parser_step = FordV0DecoderStepData;
        } else if(!level && duration > gap_threshold + 250) {
            instance->decoder.parser_step = FordV0DecoderStepReset;
        }
        break;

    case FordV0DecoderStepData: {
        ManchesterEvent event;

        if(DURATION_DIFF(duration, te_short) < te_delta) {
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        } else if(DURATION_DIFF(duration, te_long) < te_delta) {
            event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;
        } else {
            instance->decoder.parser_step = FordV0DecoderStepReset;
            break;
        }

        bool data_bit;
        if(manchester_advance(
               instance->manchester_state, event, &instance->manchester_state, &data_bit)) {
            ford_v0_add_bit(instance, data_bit);

            if(ford_v0_process_data(instance)) {
                instance->generic.data = instance->key1;
                instance->generic.data_count_bit = 64;
                instance->generic.serial = instance->serial;
                instance->generic.btn = instance->button;
                instance->generic.cnt = instance->count;

                if(instance->base.callback) {
                    instance->base.callback(&instance->base, instance->base.context);
                }

                instance->data_low = 0;
                instance->data_high = 0;
                instance->bit_count = 0;
                instance->decoder.parser_step = FordV0DecoderStepReset;
            }
        }

        instance->decoder.te_last = duration;
        break;
    }
    }
}

uint8_t subghz_protocol_decoder_ford_v0_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus subghz_protocol_decoder_ford_v0_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_serialize(&instance->generic, flipper_format, preset);

    if(ret == SubGhzProtocolStatusOk) {
        uint32_t temp = (instance->key2 >> 8) & 0xFF;
        flipper_format_write_uint32(flipper_format, "Checksum", &temp, 1);

        temp = instance->key2 & 0xFF;
        flipper_format_write_uint32(flipper_format, "CRC", &temp, 1);

        flipper_format_write_uint32(flipper_format, "Serial", &instance->serial, 1);

        temp = instance->button;
        flipper_format_write_uint32(flipper_format, "Btn", &temp, 1);

        flipper_format_write_uint32(flipper_format, "Cnt", &instance->count, 1);
    }

    return ret;
}

SubGhzProtocolStatus
    subghz_protocol_decoder_ford_v0_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;

    SubGhzProtocolStatus ret = subghz_block_generic_deserialize_check_count_bit(
        &instance->generic, flipper_format, subghz_protocol_ford_v0_const.min_count_bit_for_found);

    if(ret == SubGhzProtocolStatusOk) {
        instance->key1 = instance->generic.data;

        flipper_format_rewind(flipper_format);

        uint32_t checksum_temp = 0;
        uint32_t crc_temp = 0;
        flipper_format_read_uint32(flipper_format, "Checksum", &checksum_temp, 1);
        flipper_format_read_uint32(flipper_format, "CRC", &crc_temp, 1);
        instance->key2 = ((checksum_temp & 0xFF) << 8) | (crc_temp & 0xFF);

        flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1);
        instance->generic.serial = instance->serial;

        uint32_t btn_temp = 0;
        flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1);
        instance->button = (uint8_t)btn_temp;
        instance->generic.btn = instance->button;

        flipper_format_read_uint32(flipper_format, "Cnt", &instance->count, 1);
        instance->generic.cnt = instance->count;
    }

    return ret;
}

void subghz_protocol_decoder_ford_v0_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderFordV0* instance = context;

    uint32_t code_found_hi = (uint32_t)(instance->key1 >> 32);
    uint32_t code_found_lo = (uint32_t)(instance->key1 & 0xFFFFFFFF);

    bool crc_ok = ford_v0_verify_crc(instance->key1, instance->key2);

    const char* button_name = "??";
    if(instance->button == 0x01)
        button_name = "Panic";
    else if(instance->button == 0x02)
        button_name = "Lock";
    else if(instance->button == 0x04)
        button_name = "Unlock";
    else if(instance->button == 0x08)
        button_name = "Boot";

    furi_string_cat_printf(
        output,
        "%s %dbit CRC:%s\r\n"
        "Key1: %08lX%08lX\r\n"
        "Key2: %04X"
        "  Sn: %08lX\r\n"
        "Cnt: %05lX"
        "  Checksum: %02X"
        "  CRC: %02X\r\n"
        "  Btn: %02X - %s\r\n",
        instance->generic.protocol_name,
        instance->generic.data_count_bit,
        crc_ok ? "OK" : "BAD",
        (unsigned long)code_found_hi,
        (unsigned long)code_found_lo,
        instance->key2,
        (unsigned long)instance->serial,

        (unsigned long)instance->count,
        (instance->key2 >> 8) & 0xFF,
        instance->key2 & 0xFF,
        instance->button,
        button_name);
}
