#include "kia_v3_v4.h"
#include "../protopirate_app_i.h"
#include "keys.h"

#define TAG "KiaV3V4"

static const char* kia_version_names[] = {"Kia V4", "Kia V3"};

#define KIA_V3_V4_PREAMBLE_PAIRS     16
#define KIA_V3_V4_TOTAL_BURSTS       3
#define KIA_V3_V4_INTER_BURST_GAP_US 10000
#define KIA_V3_V4_SYNC_DURATION      1200

static const SubGhzBlockConst kia_protocol_v3_v4_const = {
    .te_short = 400,
    .te_long = 800,
    .te_delta = 150,
    .min_count_bit_for_found = 68,
};

typedef struct SubGhzProtocolDecoderKiaV3V4 {
    SubGhzProtocolDecoderBase base;
    SubGhzBlockDecoder decoder;
    SubGhzBlockGeneric generic;
    uint16_t header_count;

    uint8_t raw_bits[32];
    uint16_t raw_bit_count;
    bool is_v3_sync;

    uint32_t encrypted;
    uint32_t decrypted;
    uint8_t crc;
    uint8_t version;
} SubGhzProtocolDecoderKiaV3V4;

typedef struct SubGhzProtocolEncoderKiaV3V4 {
    SubGhzProtocolEncoderBase base;
    SubGhzProtocolBlockEncoder encoder;
    SubGhzBlockGeneric generic;

    uint32_t serial;
    uint8_t btn;
    uint16_t cnt;
    uint8_t version;

    uint32_t encrypted;
    uint32_t decrypted;
} SubGhzProtocolEncoderKiaV3V4;

typedef enum {
    KiaV3V4DecoderStepReset = 0,
    KiaV3V4DecoderStepCheckPreamble,
    KiaV3V4DecoderStepCollectRawBits,
} KiaV3V4DecoderStep;

// KeeLoq decrypt
static uint32_t keeloq_common_decrypt(uint32_t data, uint64_t key) {
    uint32_t block = data;
    uint64_t tkey = key;
    for(int i = 0; i < 528; i++) {
        int lutkey = ((block >> 0) & 1) | ((block >> 7) & 2) | ((block >> 17) & 4) |
                     ((block >> 22) & 8) | ((block >> 26) & 16);
        int lsb =
            ((block >> 31) ^ ((block >> 15) & 1) ^ ((0x3A5C742E >> lutkey) & 1) ^
             ((tkey >> 15) & 1));
        block = ((block & 0x7FFFFFFF) << 1) | lsb;
        tkey = ((tkey & 0x7FFFFFFFFFFFFFFFULL) << 1) | (tkey >> 63);
    }
    return block;
}

// KeeLoq encrypt
static uint32_t keeloq_common_encrypt(uint32_t data, uint64_t key) {
    uint32_t block = data;
    uint64_t tkey = key;

    for(int i = 0; i < 528; i++) {
        int lutkey = ((block >> 1) & 1) | ((block >> 8) & 2) | ((block >> 18) & 4) |
                     ((block >> 23) & 8) | ((block >> 27) & 16);
        int msb =
            ((block >> 0) ^ ((block >> 16) & 1) ^ ((0x3A5C742E >> lutkey) & 1) ^
             ((tkey >> 0) & 1));
        block = ((block >> 1) & 0x7FFFFFFF) | (msb << 31);
        tkey = ((tkey >> 1) & 0x7FFFFFFFFFFFFFFFULL) | ((tkey & 1) << 63);
    }
    return block;
}

static uint8_t reverse8(uint8_t byte) {
    byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
    byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
    byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
    return byte;
}

static void kia_v3_v4_add_raw_bit(SubGhzProtocolDecoderKiaV3V4* instance, bool bit) {
    if(instance->raw_bit_count < 256) {
        uint16_t byte_idx = instance->raw_bit_count / 8;
        uint8_t bit_idx = 7 - (instance->raw_bit_count % 8);
        if(bit) {
            instance->raw_bits[byte_idx] |= (1 << bit_idx);
        } else {
            instance->raw_bits[byte_idx] &= ~(1 << bit_idx);
        }
        instance->raw_bit_count++;
    }
}

static uint8_t kia_v3_v4_calculate_crc(uint8_t* bytes) {
    uint8_t crc = 0;
    for(int i = 0; i < 8; i++) {
        crc ^= (bytes[i] & 0x0F) ^ (bytes[i] >> 4);
    }
    return crc & 0x0F;
}

static bool kia_v3_v4_process_buffer(SubGhzProtocolDecoderKiaV3V4* instance) {
    if(instance->raw_bit_count < 68) {
        return false;
    }

    uint8_t* b = instance->raw_bits;

    if(instance->is_v3_sync) {
        uint16_t num_bytes = (instance->raw_bit_count + 7) / 8;
        for(uint16_t i = 0; i < num_bytes; i++) {
            b[i] = ~b[i];
        }
    }

    uint8_t crc = (b[8] >> 4) & 0x0F;

    uint32_t encrypted = ((uint32_t)reverse8(b[3]) << 24) | ((uint32_t)reverse8(b[2]) << 16) |
                         ((uint32_t)reverse8(b[1]) << 8) | (uint32_t)reverse8(b[0]);

    uint32_t serial = ((uint32_t)reverse8(b[7] & 0xF0) << 24) | ((uint32_t)reverse8(b[6]) << 16) |
                      ((uint32_t)reverse8(b[5]) << 8) | (uint32_t)reverse8(b[4]);

    uint8_t btn = (reverse8(b[7]) & 0xF0) >> 4;
    uint8_t our_serial_lsb = serial & 0xFF;

    uint32_t decrypted = keeloq_common_decrypt(encrypted, get_kia_mf_key());
    uint8_t dec_btn = (decrypted >> 28) & 0x0F;
    uint8_t dec_serial_lsb = (decrypted >> 16) & 0xFF;

    if(dec_btn != btn || dec_serial_lsb != our_serial_lsb) {
        return false;
    }

    instance->encrypted = encrypted;
    instance->decrypted = decrypted;
    instance->crc = crc;
    instance->generic.serial = serial;
    instance->generic.btn = btn;
    instance->generic.cnt = decrypted & 0xFFFF;
    instance->version = instance->is_v3_sync ? 1 : 0;

    uint64_t key_data = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) | ((uint64_t)b[2] << 40) |
                        ((uint64_t)b[3] << 32) | ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
                        ((uint64_t)b[6] << 8) | (uint64_t)b[7];
    instance->generic.data = key_data;
    instance->generic.data_count_bit = 68;

    return true;
}

const SubGhzProtocolDecoder kia_protocol_v3_v4_decoder = {
    .alloc = kia_protocol_decoder_v3_v4_alloc,
    .free = kia_protocol_decoder_v3_v4_free,
    .feed = kia_protocol_decoder_v3_v4_feed,
    .reset = kia_protocol_decoder_v3_v4_reset,
    .get_hash_data = kia_protocol_decoder_v3_v4_get_hash_data,
    .serialize = kia_protocol_decoder_v3_v4_serialize,
    .deserialize = kia_protocol_decoder_v3_v4_deserialize,
    .get_string = kia_protocol_decoder_v3_v4_get_string,
};

const SubGhzProtocolEncoder kia_protocol_v3_v4_encoder = {
    .alloc = kia_protocol_encoder_v3_v4_alloc,
    .free = kia_protocol_encoder_v3_v4_free,
    .deserialize = kia_protocol_encoder_v3_v4_deserialize,
    .stop = kia_protocol_encoder_v3_v4_stop,
    .yield = kia_protocol_encoder_v3_v4_yield,
};

const SubGhzProtocol kia_protocol_v3_v4 = {
    .name = KIA_PROTOCOL_V3_V4_NAME,
    .type = SubGhzProtocolTypeDynamic,
    .flag = SubGhzProtocolFlag_315 | SubGhzProtocolFlag_433 | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable | SubGhzProtocolFlag_Load | SubGhzProtocolFlag_Save |
            SubGhzProtocolFlag_Send,
    .decoder = &kia_protocol_v3_v4_decoder,
    .encoder = &kia_protocol_v3_v4_encoder,
};

// ============================================================================
// ENCODER IMPLEMENTATION
// ============================================================================

void* kia_protocol_encoder_v3_v4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolEncoderKiaV3V4* instance = malloc(sizeof(SubGhzProtocolEncoderKiaV3V4));

    instance->base.protocol = &kia_protocol_v3_v4;
    instance->generic.protocol_name = instance->base.protocol->name;

    instance->serial = 0;
    instance->btn = 0;
    instance->cnt = 0;
    instance->version = 0;

    instance->encoder.size_upload = 600;
    instance->encoder.upload = malloc(instance->encoder.size_upload * sizeof(LevelDuration));
    instance->encoder.repeat = 40;
    instance->encoder.front = 0;
    instance->encoder.is_running = false;

    FURI_LOG_I(TAG, "Encoder allocated at %p", instance);
    return instance;
}

void kia_protocol_encoder_v3_v4_free(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    if(instance->encoder.upload) {
        free(instance->encoder.upload);
    }
    free(instance);
}

static void kia_protocol_encoder_v3_v4_build_packet(
    SubGhzProtocolEncoderKiaV3V4* instance,
    uint8_t* raw_bytes) {
    // Build plaintext for encryption:
    uint32_t plaintext = (instance->cnt & 0xFFFF) | ((instance->serial & 0xFF) << 16) |
                         (0x1 << 24) | ((instance->btn & 0x0F) << 28);

    instance->decrypted = plaintext;

    uint32_t encrypted = keeloq_common_encrypt(plaintext, get_kia_mf_key());
    instance->encrypted = encrypted;

    FURI_LOG_I(
        TAG,
        "Encrypt: plain=0x%08lX -> enc=0x%08lX",
        (unsigned long)plaintext,
        (unsigned long)encrypted);

    // Decoder does: encrypted = (rev(b[3])<<24) | (rev(b[2])<<16) | (rev(b[1])<<8) | rev(b[0])
    // So b[0] must contain reverse8(LSB), b[3] must contain reverse8(MSB)
    raw_bytes[0] = reverse8((encrypted >> 0) & 0xFF); // LSB
    raw_bytes[1] = reverse8((encrypted >> 8) & 0xFF);
    raw_bytes[2] = reverse8((encrypted >> 16) & 0xFF);
    raw_bytes[3] = reverse8((encrypted >> 24) & 0xFF); // MSB

    // Serial/button
    uint32_t serial_btn = (instance->serial & 0x0FFFFFFF) |
                          ((uint32_t)(instance->btn & 0x0F) << 28);
    raw_bytes[4] = reverse8((serial_btn >> 0) & 0xFF);
    raw_bytes[5] = reverse8((serial_btn >> 8) & 0xFF);
    raw_bytes[6] = reverse8((serial_btn >> 16) & 0xFF);
    raw_bytes[7] = reverse8((serial_btn >> 24) & 0xFF);

    // CRC
    uint8_t crc = kia_v3_v4_calculate_crc(raw_bytes);
    raw_bytes[8] = (crc << 4);

    // DEBUG: Log the exact raw bytes we're generating
    FURI_LOG_I(
        TAG,
        "TX raw: %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        raw_bytes[0],
        raw_bytes[1],
        raw_bytes[2],
        raw_bytes[3],
        raw_bytes[4],
        raw_bytes[5],
        raw_bytes[6],
        raw_bytes[7],
        raw_bytes[8]);

    // Store in generic.data for display
    instance->generic.data = ((uint64_t)raw_bytes[0] << 56) | ((uint64_t)raw_bytes[1] << 48) |
                             ((uint64_t)raw_bytes[2] << 40) | ((uint64_t)raw_bytes[3] << 32) |
                             ((uint64_t)raw_bytes[4] << 24) | ((uint64_t)raw_bytes[5] << 16) |
                             ((uint64_t)raw_bytes[6] << 8) | (uint64_t)raw_bytes[7];
    instance->generic.data_count_bit = 68;

    FURI_LOG_I(
        TAG,
        "Packet built: Serial=0x%07lX, Btn=0x%X, Cnt=0x%04X, CRC=0x%X",
        (unsigned long)instance->serial,
        instance->btn,
        instance->cnt,
        crc);
}

static void kia_protocol_encoder_v3_v4_get_upload(SubGhzProtocolEncoderKiaV3V4* instance) {
    furi_check(instance);

    uint8_t raw_bytes[9];
    kia_protocol_encoder_v3_v4_build_packet(instance, raw_bytes);

    if(instance->version == 1) {
        for(int i = 0; i < 9; i++) {
            raw_bytes[i] = ~raw_bytes[i];
        }
    }

    size_t index = 0;

    for(uint8_t burst = 0; burst < KIA_V3_V4_TOTAL_BURSTS; burst++) {
        if(burst > 0) {
            instance->encoder.upload[index++] =
                level_duration_make(false, KIA_V3_V4_INTER_BURST_GAP_US);
        }

        // Preamble: alternating short pulses
        for(int i = 0; i < KIA_V3_V4_PREAMBLE_PAIRS; i++) {
            instance->encoder.upload[index++] =
                level_duration_make(true, kia_protocol_v3_v4_const.te_short);
            instance->encoder.upload[index++] =
                level_duration_make(false, kia_protocol_v3_v4_const.te_short);
        }

        // Sync pulse - different for V3 vs V4
        if(instance->version == 0) {
            // V4: long HIGH, short LOW
            instance->encoder.upload[index++] = level_duration_make(true, KIA_V3_V4_SYNC_DURATION);
            instance->encoder.upload[index++] =
                level_duration_make(false, kia_protocol_v3_v4_const.te_short);
        } else {
            // V3: short HIGH, long LOW
            instance->encoder.upload[index++] =
                level_duration_make(true, kia_protocol_v3_v4_const.te_short);
            instance->encoder.upload[index++] =
                level_duration_make(false, KIA_V3_V4_SYNC_DURATION);
        }

        // Data bits - PWM encoding with complementary durations
        for(int byte_idx = 0; byte_idx < 9; byte_idx++) {
            int bits_in_byte = (byte_idx == 8) ? 4 : 8;

            for(int bit_idx = 7; bit_idx >= (8 - bits_in_byte); bit_idx--) {
                bool bit = (raw_bytes[byte_idx] >> bit_idx) & 1;

                if(bit) {
                    // bit 1: long HIGH, short LOW (total ~1200µs)
                    instance->encoder.upload[index++] =
                        level_duration_make(true, kia_protocol_v3_v4_const.te_long); // 800µs
                    instance->encoder.upload[index++] =
                        level_duration_make(false, kia_protocol_v3_v4_const.te_short); // 400µs
                } else {
                    // bit 0: short HIGH, long LOW (total ~1200µs)
                    instance->encoder.upload[index++] =
                        level_duration_make(true, kia_protocol_v3_v4_const.te_short); // 400µs
                    instance->encoder.upload[index++] =
                        level_duration_make(false, kia_protocol_v3_v4_const.te_long); // 800µs
                }
            }
        }
    }

    //instance->encoder.upload[index++] = level_duration_make(false, KIA_V3_V4_INTER_BURST_GAP_US);

    instance->encoder.size_upload = index;
    instance->encoder.front = 0;

    FURI_LOG_I(
        TAG,
        "Upload built: %d bursts, size_upload=%zu, version=%s",
        KIA_V3_V4_TOTAL_BURSTS,
        instance->encoder.size_upload,
        instance->version == 0 ? "V4" : "V3");
}

SubGhzProtocolStatus
    kia_protocol_encoder_v3_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;

    instance->encoder.is_running = false;
    instance->encoder.front = 0;
    //instance->encoder.repeat = 40;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    flipper_format_rewind(flipper_format);

    do {
        FuriString* temp_str = furi_string_alloc();
        if(!flipper_format_read_string(flipper_format, "Protocol", temp_str)) {
            FURI_LOG_E(TAG, "Missing Protocol");
            furi_string_free(temp_str);
            break;
        }

        // Accept "Kia V3/V4", "Kia V3", or "Kia V4"
        const char* proto_str = furi_string_get_cstr(temp_str);
        if(!furi_string_equal(temp_str, instance->base.protocol->name) &&
           strcmp(proto_str, "Kia V3") != 0 && strcmp(proto_str, "Kia V4") != 0) {
            FURI_LOG_E(TAG, "Wrong protocol %s", proto_str);
            furi_string_free(temp_str);
            break;
        }

        // Set version based on protocol name if specific
        bool version_from_protocol_name = false;

        if(strcmp(proto_str, "Kia V3") == 0) {
            instance->version = 1;
            version_from_protocol_name = true;
            FURI_LOG_I(TAG, "Protocol name indicates V3");
        } else if(strcmp(proto_str, "Kia V4") == 0) {
            instance->version = 0;
            version_from_protocol_name = true;
            FURI_LOG_I(TAG, "Protocol name indicates V4");
        }

        furi_string_free(temp_str);

        // Read bit count
        flipper_format_rewind(flipper_format);
        uint32_t bit_count_temp;
        if(!flipper_format_read_uint32(flipper_format, "Bit", &bit_count_temp, 1)) {
            FURI_LOG_E(TAG, "Missing Bit");
            break;
        }
        instance->generic.data_count_bit = 68;

        // Read key data
        flipper_format_rewind(flipper_format);
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
                break;
            }

            key = (key << 4) | nibble;
            hex_pos++;
        }

        furi_string_free(temp_str);

        instance->generic.data = key;
        FURI_LOG_I(TAG, "Parsed key: 0x%016llX", (unsigned long long)instance->generic.data);

        // Read serial
        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(flipper_format, "Serial", &instance->serial, 1)) {
            uint8_t b[8];
            b[0] = (key >> 56) & 0xFF;
            b[1] = (key >> 48) & 0xFF;
            b[2] = (key >> 40) & 0xFF;
            b[3] = (key >> 32) & 0xFF;
            b[4] = (key >> 24) & 0xFF;
            b[5] = (key >> 16) & 0xFF;
            b[6] = (key >> 8) & 0xFF;
            b[7] = key & 0xFF;

            instance->serial = ((uint32_t)reverse8(b[7] & 0xF0) << 24) |
                               ((uint32_t)reverse8(b[6]) << 16) | ((uint32_t)reverse8(b[5]) << 8) |
                               (uint32_t)reverse8(b[4]);
            FURI_LOG_I(TAG, "Extracted serial: 0x%08lX", (unsigned long)instance->serial);
        } else {
            FURI_LOG_I(TAG, "Read serial: 0x%08lX", (unsigned long)instance->serial);
        }
        instance->generic.serial = instance->serial;

        // Read button
        flipper_format_rewind(flipper_format);
        uint32_t btn_temp;
        if(flipper_format_read_uint32(flipper_format, "Btn", &btn_temp, 1)) {
            instance->btn = (uint8_t)btn_temp;
            FURI_LOG_I(TAG, "Read button: 0x%X", instance->btn);
        } else {
            uint8_t b7 = instance->generic.data & 0xFF;
            instance->btn = (reverse8(b7) & 0xF0) >> 4;
            FURI_LOG_I(TAG, "Extracted button: 0x%X", instance->btn);
        }
        instance->generic.btn = instance->btn;

        // Read counter
        flipper_format_rewind(flipper_format);
        uint32_t cnt_temp;
        if(flipper_format_read_uint32(flipper_format, "Cnt", &cnt_temp, 1)) {
            instance->cnt = (uint16_t)cnt_temp;
            FURI_LOG_I(TAG, "Read counter: 0x%04X", instance->cnt);
        } else {
            // Try Decrypted field
            flipper_format_rewind(flipper_format);
            uint32_t decrypted_temp;
            if(flipper_format_read_uint32(flipper_format, "Decrypted", &decrypted_temp, 1)) {
                instance->cnt = decrypted_temp & 0xFFFF;
                FURI_LOG_I(TAG, "Extracted counter from Decrypted: 0x%04X", instance->cnt);
            } else {
                instance->cnt = 0;
                FURI_LOG_W(TAG, "Counter not found, using 0");
            }
        }
        instance->generic.cnt = instance->cnt;

        // Read version - ONLY use file version if protocol name didn't specify one
        flipper_format_rewind(flipper_format);
        uint32_t version_temp;
        if(flipper_format_read_uint32(flipper_format, "KIAVersion", &version_temp, 1)) {
            if(!version_from_protocol_name) {
                // Generic "Kia V3/V4" protocol - use file's version
                instance->version = (uint8_t)version_temp;
                FURI_LOG_I(
                    TAG, "Read version from file: %s", instance->version == 0 ? "V4" : "V3");
            } else {
                // Protocol name was specific - trust that, ignore file version
                FURI_LOG_I(
                    TAG,
                    "Using version from protocol name: %s (file had Version=%lu)",
                    instance->version == 0 ? "V4" : "V3",
                    (unsigned long)version_temp);
            }
        } else if(!version_from_protocol_name) {
            instance->version = 0;
            FURI_LOG_I(TAG, "Version not found, defaulting to V4");
        }

        // Read repeat
        flipper_format_rewind(flipper_format);
        if(!flipper_format_read_uint32(
               flipper_format, "Repeat", (uint32_t*)&instance->encoder.repeat, 1)) {
            instance->encoder.repeat = 40;
            FURI_LOG_D(TAG, "Repeat not found, using default 40");
        }

        // Build the upload
        kia_protocol_encoder_v3_v4_get_upload(instance);

        instance->encoder.is_running = true;
        instance->encoder.front = 0;

        FURI_LOG_I(
            TAG,
            "Encoder initialized: Serial=0x%07lX, Btn=0x%X, Cnt=0x%04X, Version=%s",
            (unsigned long)instance->serial,
            instance->btn,
            instance->cnt,
            instance->version == 0 ? "V4" : "V3");

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

void kia_protocol_encoder_v3_v4_stop(void* context) {
    if(!context) return;
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    instance->encoder.is_running = false;
    instance->encoder.front = 0;
}

LevelDuration kia_protocol_encoder_v3_v4_yield(void* context) {
    SubGhzProtocolEncoderKiaV3V4* instance = context;

    if(!instance || !instance->encoder.upload || instance->encoder.repeat == 0 ||
       !instance->encoder.is_running) {
        if(instance) {
            FURI_LOG_D(
                TAG,
                "Encoder yield stopped: repeat=%u, is_running=%d",
                instance->encoder.repeat,
                instance->encoder.is_running);
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

    if(instance->encoder.front < 5) {
        FURI_LOG_D(
            TAG,
            "Encoder yield[%zu]: repeat=%u, level=%d, duration=%lu",
            instance->encoder.front,
            instance->encoder.repeat,
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

void kia_protocol_encoder_v3_v4_set_button(void* context, uint8_t button) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    instance->btn = button & 0x0F;
    instance->generic.btn = instance->btn;
    kia_protocol_encoder_v3_v4_get_upload(instance);
    FURI_LOG_I(TAG, "Button set to 0x%X", instance->btn);
}

void kia_protocol_encoder_v3_v4_set_counter(void* context, uint16_t counter) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    instance->cnt = counter;
    instance->generic.cnt = instance->cnt;
    kia_protocol_encoder_v3_v4_get_upload(instance);
    FURI_LOG_I(TAG, "Counter set to 0x%04X", instance->cnt);
}

void kia_protocol_encoder_v3_v4_increment_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    instance->cnt++;
    instance->generic.cnt = instance->cnt;
    kia_protocol_encoder_v3_v4_get_upload(instance);
    FURI_LOG_I(TAG, "Counter incremented to 0x%04X", instance->cnt);
}

uint16_t kia_protocol_encoder_v3_v4_get_counter(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    return instance->cnt;
}

uint8_t kia_protocol_encoder_v3_v4_get_button(void* context) {
    furi_check(context);
    SubGhzProtocolEncoderKiaV3V4* instance = context;
    return instance->btn;
}

// ============================================================================
// DECODER IMPLEMENTATION
// ============================================================================

void* kia_protocol_decoder_v3_v4_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    SubGhzProtocolDecoderKiaV3V4* instance = malloc(sizeof(SubGhzProtocolDecoderKiaV3V4));
    instance->base.protocol = &kia_protocol_v3_v4;
    instance->generic.protocol_name = instance->base.protocol->name;
    return instance;
}

void kia_protocol_decoder_v3_v4_free(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;
    free(instance);
}

void kia_protocol_decoder_v3_v4_reset(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;
    instance->decoder.parser_step = KiaV3V4DecoderStepReset;
    instance->header_count = 0;
    instance->raw_bit_count = 0;
    instance->crc = 0;
    memset(instance->raw_bits, 0, sizeof(instance->raw_bits));
}

void kia_protocol_decoder_v3_v4_feed(void* context, bool level, uint32_t duration) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    switch(instance->decoder.parser_step) {
    case KiaV3V4DecoderStepReset:
        if(level && (DURATION_DIFF(duration, kia_protocol_v3_v4_const.te_short) <
                     kia_protocol_v3_v4_const.te_delta)) {
            instance->decoder.parser_step = KiaV3V4DecoderStepCheckPreamble;
            instance->decoder.te_last = duration;
            instance->header_count = 1;
        }
        break;

    case KiaV3V4DecoderStepCheckPreamble:
        if(level) {
            if(DURATION_DIFF(duration, kia_protocol_v3_v4_const.te_short) <
               kia_protocol_v3_v4_const.te_delta) {
                instance->decoder.te_last = duration;
            } else if(duration > 1000 && duration < 1500) {
                if(instance->header_count >= 8) {
                    instance->decoder.parser_step = KiaV3V4DecoderStepCollectRawBits;
                    instance->raw_bit_count = 0;
                    instance->is_v3_sync = false;
                    memset(instance->raw_bits, 0, sizeof(instance->raw_bits));
                } else {
                    instance->decoder.parser_step = KiaV3V4DecoderStepReset;
                }
            } else {
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            }
        } else {
            if(duration > 1000 && duration < 1500) {
                if(instance->header_count >= 8) {
                    instance->decoder.parser_step = KiaV3V4DecoderStepCollectRawBits;
                    instance->raw_bit_count = 0;
                    instance->is_v3_sync = true;
                    memset(instance->raw_bits, 0, sizeof(instance->raw_bits));
                } else {
                    instance->decoder.parser_step = KiaV3V4DecoderStepReset;
                }
            } else if(
                (DURATION_DIFF(duration, kia_protocol_v3_v4_const.te_short) <
                 kia_protocol_v3_v4_const.te_delta) &&
                (DURATION_DIFF(instance->decoder.te_last, kia_protocol_v3_v4_const.te_short) <
                 kia_protocol_v3_v4_const.te_delta)) {
                instance->header_count++;
            } else if(duration > 1500) {
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            }
        }
        break;

    case KiaV3V4DecoderStepCollectRawBits:
        if(level) {
            if(duration > 1000 && duration < 1500) {
                if(kia_v3_v4_process_buffer(instance)) {
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            } else if(
                DURATION_DIFF(duration, kia_protocol_v3_v4_const.te_short) <
                kia_protocol_v3_v4_const.te_delta) {
                kia_v3_v4_add_raw_bit(instance, false);
            } else if(
                DURATION_DIFF(duration, kia_protocol_v3_v4_const.te_long) <
                kia_protocol_v3_v4_const.te_delta) {
                kia_v3_v4_add_raw_bit(instance, true);
            } else {
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            }
        } else {
            if(duration > 1000 && duration < 1500) {
                if(kia_v3_v4_process_buffer(instance)) {
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            } else if(duration > 1500) {
                if(kia_v3_v4_process_buffer(instance)) {
                    if(instance->base.callback)
                        instance->base.callback(&instance->base, instance->base.context);
                }
                instance->decoder.parser_step = KiaV3V4DecoderStepReset;
            }
        }
        break;
    }
}

uint8_t kia_protocol_decoder_v3_v4_get_hash_data(void* context) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;
    return subghz_protocol_blocks_get_hash_data(
        &instance->decoder, (instance->decoder.decode_count_bit / 8) + 1);
}

SubGhzProtocolStatus kia_protocol_decoder_v3_v4_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    SubGhzProtocolStatus ret = SubGhzProtocolStatusError;

    do {
        // Write frequency
        if(!flipper_format_write_uint32(flipper_format, "Frequency", &preset->frequency, 1)) {
            break;
        }

        // Write preset
        if(!flipper_format_write_string_cstr(
               flipper_format, "Preset", furi_string_get_cstr(preset->name))) {
            break;
        }

        // Write version-specific protocol name instead of generic "Kia V3/V4"
        const char* version_name = (instance->version == 0) ? "Kia V4" : "Kia V3";
        if(!flipper_format_write_string_cstr(flipper_format, "Protocol", version_name)) {
            break;
        }

        // Write bit count
        uint32_t bits = instance->generic.data_count_bit;
        if(!flipper_format_write_uint32(flipper_format, "Bit", &bits, 1)) {
            break;
        }

        // Write key
        char key_str[20];
        snprintf(key_str, sizeof(key_str), "%016llX", (unsigned long long)instance->generic.data);
        if(!flipper_format_write_string_cstr(flipper_format, "Key", key_str)) {
            break;
        }

        // Write all fields needed by encoder
        if(!flipper_format_write_uint32(flipper_format, "Serial", &instance->generic.serial, 1)) {
            break;
        }

        uint32_t temp = instance->generic.btn;
        if(!flipper_format_write_uint32(flipper_format, "Btn", &temp, 1)) {
            break;
        }

        temp = instance->generic.cnt;
        if(!flipper_format_write_uint32(flipper_format, "Cnt", &temp, 1)) {
            break;
        }

        // Write protocol-specific fields
        if(!flipper_format_write_uint32(flipper_format, "Encrypted", &instance->encrypted, 1)) {
            break;
        }

        if(!flipper_format_write_uint32(flipper_format, "Decrypted", &instance->decrypted, 1)) {
            break;
        }

        temp = instance->version;
        if(!flipper_format_write_uint32(flipper_format, "KIAVersion", &temp, 1)) {
            break;
        }

        temp = instance->crc;
        if(!flipper_format_write_uint32(flipper_format, "CRC", &temp, 1)) {
            break;
        }

        ret = SubGhzProtocolStatusOk;
    } while(false);

    return ret;
}

SubGhzProtocolStatus
    kia_protocol_decoder_v3_v4_deserialize(void* context, FlipperFormat* flipper_format) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    SubGhzProtocolStatus ret =
        subghz_block_generic_deserialize_check_count_bit(&instance->generic, flipper_format, 64);

    if(ret == SubGhzProtocolStatusOk) {
        uint32_t temp = 0;

        if(flipper_format_read_uint32(flipper_format, "Encrypted", &temp, 1)) {
            instance->encrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "Decrypted", &temp, 1)) {
            instance->decrypted = temp;
        }
        if(flipper_format_read_uint32(flipper_format, "KIAVersion", &temp, 1)) {
            instance->version = (uint8_t)temp;
        }
        if(flipper_format_read_uint32(flipper_format, "CRC", &temp, 1)) {
            instance->crc = (uint8_t)temp;
        }
    }

    return ret;
}

static uint64_t compute_yek(uint64_t key) {
    uint64_t yek = 0;
    for(int i = 0; i < 64; i++) {
        yek |= ((key >> i) & 1) << (63 - i);
    }
    return yek;
}

void kia_protocol_decoder_v3_v4_get_string(void* context, FuriString* output) {
    furi_check(context);
    SubGhzProtocolDecoderKiaV3V4* instance = context;

    uint64_t yek = compute_yek(instance->generic.data);
    uint32_t key_hi = (uint32_t)(instance->generic.data >> 32);
    uint32_t key_lo = (uint32_t)(instance->generic.data & 0xFFFFFFFF);
    uint32_t yek_hi = (uint32_t)(yek >> 32);
    uint32_t yek_lo = (uint32_t)(yek & 0xFFFFFFFF);

    if(instance->version == 0) {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:%08lX%08lX\r\n"
            "Yek:%08lX%08lX\r\n"
            "Serial:%07lX Btn:%01X CRC:%01X\r\n"
            "Decr:%08lX Cnt:%04lX\r\n",
            kia_version_names[instance->version],
            instance->generic.data_count_bit,
            key_hi,
            key_lo,
            yek_hi,
            yek_lo,
            instance->generic.serial,
            instance->generic.btn,
            instance->crc,
            instance->decrypted,
            instance->generic.cnt);
    } else {
        furi_string_cat_printf(
            output,
            "%s %dbit\r\n"
            "Key:%08lX%08lX\r\n"
            "Yek:%08lX%08lX\r\n"
            "Serial:%07lX Btn:%01X\r\n"
            "Decr:%08lX Cnt:%04lX\r\n",
            kia_version_names[instance->version],
            instance->generic.data_count_bit,
            key_hi,
            key_lo,
            yek_hi,
            yek_lo,
            instance->generic.serial,
            instance->generic.btn,
            instance->decrypted,
            instance->generic.cnt);
    }
}
