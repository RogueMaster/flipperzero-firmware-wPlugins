#include "../ami_tool_i.h"

#include <stdbool.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <string.h>
#include <stdlib.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

#define AMIIBO_PAGE_COUNT (135U)
#define AMIIBO_TOTAL_BYTES (AMIIBO_PAGE_COUNT * MF_ULTRALIGHT_PAGE_SIZE)
#define AMIIBO_OFFSET_CAPABILITY (0x0CU)
#define AMIIBO_OFFSET_FIXED_A5 (0x10U)
#define AMIIBO_OFFSET_WRITE_COUNTER (0x11U)
#define AMIIBO_WRITE_COUNTER_SIZE (2U)
#define AMIIBO_OFFSET_TAG_CONFIG (0x14U)
#define AMIIBO_TAG_CONFIG_SIZE (32U)
#define AMIIBO_OFFSET_TAG_HASH (0x34U)
#define AMIIBO_HASH_SIZE (32U)
#define AMIIBO_OFFSET_MODEL_INFO (0x54U)
#define AMIIBO_MODEL_INFO_SIZE (12U)
#define AMIIBO_OFFSET_KEYGEN_SALT (0x60U)
#define AMIIBO_KEYGEN_SALT_SIZE (32U)
#define AMIIBO_OFFSET_DATA_HASH (0x80U)
#define AMIIBO_OFFSET_APP_DATA (0xA0U)
#define AMIIBO_APP_DATA_SIZE (360U)
#define AMIIBO_OFFSET_AUTH_MAGIC (0x218U)
#define AMIIBO_AUTH_MAGIC_SIZE (2U)
#define AMIIBO_OFFSET_DYNAMIC_LOCK (0x208U)
#define AMIIBO_OFFSET_RESERVED (0x20BU)
#define AMIIBO_OFFSET_CONFIG (0x20CU)
#define AMIIBO_CONFIG_SIZE (16U)
#define AMIIBO_SIGNING_BUFFER_SIZE (480U)
#define AMIIBO_MAX_SEED_SIZE (480U)

static const uint8_t amiibo_ntag215_atqa[2] = {0x44, 0x00};
static const uint8_t amiibo_ntag215_sak = 0x00;
static const MfUltralightVersion amiibo_ntag215_version = {
    .header = 0x00,
    .vendor_id = 0x04,
    .prod_type = 0x04,
    .prod_subtype = 0x02,
    .prod_ver_major = 0x01,
    .prod_ver_minor = 0x00,
    .storage_size = 0x11,
    .protocol_type = 0x03,
};
static const uint8_t amiibo_ntag215_signature[NTAG_SIGNATURE_SIZE] = {0};

static void amiibo_apply_ntag215_metadata(MfUltralightData* tag_data) {
    if(!tag_data) {
        return;
    }

    tag_data->version = amiibo_ntag215_version;
    memcpy(tag_data->signature.data, amiibo_ntag215_signature, sizeof(amiibo_ntag215_signature));

    for(size_t i = 0; i < MF_ULTRALIGHT_COUNTER_NUM; i++) {
        tag_data->counter[i].counter = 0;
        tag_data->tearing_flag[i].data = 0x00;
    }
}

static size_t amiibo_strnlen_safe(const char* str, size_t max_len) {
    size_t len = 0;
    if(!str) {
        return 0;
    }
    while((len < max_len) && str[len]) {
        len++;
    }
    return len;
}

static void amiibo_update_uid_checksums(uint8_t* raw) {
    if(!raw) {
        return;
    }
    raw[3] = (uint8_t)(raw[0] ^ raw[1] ^ raw[2] ^ 0x88);
    raw[8] = (uint8_t)(raw[4] ^ raw[5] ^ raw[6] ^ raw[7]);
}

static void amiibo_apply_uid_bytes(uint8_t* raw, const uint8_t* uid) {
    if(!raw || !uid) {
        return;
    }
    raw[0] = uid[0];
    raw[1] = uid[1];
    raw[2] = uid[2];
    raw[4] = uid[3];
    raw[5] = uid[4];
    raw[6] = uid[5];
    raw[7] = uid[6];
    amiibo_update_uid_checksums(raw);
}

static inline uint8_t* amiibo_bytes(MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

static inline const uint8_t* amiibo_bytes_const(const MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

void amiibo_configure_rf_interface(MfUltralightData* tag_data) {
    if(!tag_data) {
        return;
    }

    if(tag_data->version.vendor_id == 0 && tag_data->version.storage_size == 0) {
        amiibo_apply_ntag215_metadata(tag_data);
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    uint8_t uid[ISO14443_3A_UID_7_BYTES];
    uid[0] = raw[0];
    uid[1] = raw[1];
    uid[2] = raw[2];
    uid[3] = raw[4];
    uid[4] = raw[5];
    uid[5] = raw[6];
    uid[6] = raw[7];
    mf_ultralight_set_uid(tag_data, uid, sizeof(uid));

    Iso14443_3aData* iso = mf_ultralight_get_base_data(tag_data);
    if(iso) {
        iso14443_3a_set_atqa(iso, amiibo_ntag215_atqa);
        iso14443_3a_set_sak(iso, amiibo_ntag215_sak);
    }
}

static bool amiibo_has_full_dump(const MfUltralightData* tag_data) {
    return (
        tag_data && (tag_data->type == MfUltralightTypeNTAG215) &&
        (tag_data->pages_total >= AMIIBO_PAGE_COUNT));
}

static void amiibo_derive_step(
    bool* used,
    uint16_t* iteration,
    uint8_t* buffer,
    size_t buffer_size,
    mbedtls_md_context_t* hmac_context,
    uint8_t* output) {
    if(*used) {
        mbedtls_md_hmac_reset(hmac_context);
    } else {
        *used = true;
    }

    buffer[0] = (uint8_t)(*iteration >> 8);
    buffer[1] = (uint8_t)(*iteration);
    (*iteration)++;

    mbedtls_md_hmac_update(hmac_context, buffer, buffer_size);
    mbedtls_md_hmac_finish(hmac_context, output);
}

static RfidxStatus amiibo_randomize_uid(uint8_t* raw) {
    if(!raw) return RFIDX_ARGUMENT_ERROR;

    uint8_t uid[7];
    uid[0] = 0x04;
    uint8_t buffer[6];
    furi_hal_random_fill_buf(buffer, sizeof(buffer));

    uid[1] = buffer[0];
    uid[2] = buffer[1];

    for(size_t i = 0; i < 4; i++) {
        uid[3 + i] = buffer[i + 2];
    }
    amiibo_apply_uid_bytes(raw, uid);

    return RFIDX_OK;
}

static void amiibo_calculate_password(const uint8_t* manufacturer, uint8_t* password_out) {
    password_out[0] = manufacturer[1] ^ manufacturer[4] ^ 0xAA;
    password_out[1] = manufacturer[2] ^ manufacturer[5] ^ 0x55;
    password_out[2] = manufacturer[4] ^ manufacturer[6] ^ 0xAA;
    password_out[3] = manufacturer[5] ^ manufacturer[7] ^ 0x55;
}

static void amiibo_finalize_uid_update(MfUltralightData* tag_data) {
    if(!tag_data) {
        return;
    }
    uint8_t* raw = amiibo_bytes(tag_data);

    uint8_t password[4];
    amiibo_calculate_password(raw, password);
    memcpy(raw + AMIIBO_OFFSET_CONFIG + 8, password, sizeof(password));
    raw[AMIIBO_OFFSET_CONFIG + 12] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 13] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 14] = 0x00;
    raw[AMIIBO_OFFSET_CONFIG + 15] = 0x00;

    amiibo_configure_rf_interface(tag_data);
}

RfidxStatus amiibo_derive_key(
    const DumpedKeySingle* input_key,
    const MfUltralightData* tag_data,
    DerivedKey* derived_key) {
    if(!input_key || !tag_data || !derived_key) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if((input_key->magicBytesSize != 14U) && (input_key->magicBytesSize != 16U)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    const uint8_t* raw = amiibo_bytes_const(tag_data);

    uint8_t* prepared_seed = malloc(AMIIBO_MAX_SEED_SIZE);
    if(!prepared_seed) {
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(prepared_seed, 0, AMIIBO_MAX_SEED_SIZE);
    size_t type_len = amiibo_strnlen_safe(input_key->typeString, sizeof(input_key->typeString));
    if(type_len >= sizeof(input_key->typeString)) {
        type_len = sizeof(input_key->typeString) - 1;
    }
    memcpy(prepared_seed, input_key->typeString, type_len);
    prepared_seed[type_len] = '\0';
    uint8_t* curr = prepared_seed + type_len + 1;

    const size_t leading_seed_bytes = 16U - input_key->magicBytesSize;
    memcpy(curr, raw + AMIIBO_OFFSET_WRITE_COUNTER, leading_seed_bytes);
    curr += leading_seed_bytes;
    memcpy(curr, input_key->magicBytes, input_key->magicBytesSize);
    curr += input_key->magicBytesSize;

    memcpy(curr, raw, 8);
    memcpy(curr + 8, raw, 8);
    curr += 16;

    for(size_t i = 0; i < AMIIBO_KEYGEN_SALT_SIZE; i++) {
        curr[i] = raw[AMIIBO_OFFSET_KEYGEN_SALT + i] ^ input_key->xorTable[i];
    }
    curr += AMIIBO_KEYGEN_SALT_SIZE;

    const size_t prepared_seed_size = curr - prepared_seed;
    uint8_t* buffer = malloc(sizeof(uint16_t) + AMIIBO_MAX_SEED_SIZE);
    if(!buffer) {
        free(prepared_seed);
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(buffer, 0, sizeof(uint16_t) + AMIIBO_MAX_SEED_SIZE);
    memcpy(buffer + sizeof(uint16_t), prepared_seed, prepared_seed_size);
    const size_t buffer_size = sizeof(uint16_t) + prepared_seed_size;

    mbedtls_md_context_t hmac_context;
    mbedtls_md_init(&hmac_context);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(mbedtls_md_setup(&hmac_context, md_info, 1) != 0) {
        free(buffer);
        free(prepared_seed);
        mbedtls_md_free(&hmac_context);
        return RFIDX_ARGUMENT_ERROR;
    }
    mbedtls_md_hmac_starts(&hmac_context, input_key->hmacKey, sizeof(input_key->hmacKey));

    size_t remaining = sizeof(DerivedKey);
    uint8_t* out = (uint8_t*)derived_key;
    bool used = false;
    uint16_t iteration = 0;

    while(remaining > 0) {
        if(remaining < 32U) {
            uint8_t temp[32];
            amiibo_derive_step(&used, &iteration, buffer, buffer_size, &hmac_context, temp);
            memcpy(out, temp, remaining);
            remaining = 0;
        } else {
            amiibo_derive_step(&used, &iteration, buffer, buffer_size, &hmac_context, out);
            out += 32;
            remaining -= 32;
        }
    }

    mbedtls_md_free(&hmac_context);
    free(buffer);
    free(prepared_seed);

    return RFIDX_OK;
}

RfidxStatus amiibo_cipher(const DerivedKey* data_key, MfUltralightData* tag_data) {
    if(!data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    uint8_t* tag_cfg = raw + AMIIBO_OFFSET_TAG_CONFIG;
    uint8_t* app_data = raw + AMIIBO_OFFSET_APP_DATA;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, data_key->aesKey, 128);

    uint8_t nonce_counter[16] = {0};
    uint8_t stream_block[16] = {0};
    memcpy(nonce_counter, data_key->aesIV, sizeof(nonce_counter));
    size_t nc_offset = 0;

    // CTR stream must cover both encrypted regions consecutively even though they are
    // non-contiguous in raw tag memory.
    mbedtls_aes_crypt_ctr(
        &aes, AMIIBO_TAG_CONFIG_SIZE, &nc_offset, nonce_counter, stream_block, tag_cfg, tag_cfg);
    mbedtls_aes_crypt_ctr(
        &aes, AMIIBO_APP_DATA_SIZE, &nc_offset, nonce_counter, stream_block, app_data, app_data);

    mbedtls_aes_free(&aes);

    return RFIDX_OK;
}

RfidxStatus amiibo_generate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data,
    uint8_t* tag_hash,
    uint8_t* data_hash) {
    if(!tag_key || !data_key || !tag_data || !tag_hash || !data_hash) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* signing_buffer = malloc(AMIIBO_SIGNING_BUFFER_SIZE);
    if(!signing_buffer) {
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(signing_buffer, 0, AMIIBO_SIGNING_BUFFER_SIZE);
    const uint8_t* raw = amiibo_bytes_const(tag_data);

    memcpy(signing_buffer, raw + AMIIBO_OFFSET_FIXED_A5, 36);
    memcpy(signing_buffer + 36, raw + AMIIBO_OFFSET_APP_DATA, AMIIBO_APP_DATA_SIZE);
    memcpy(signing_buffer + 428, raw, 8);
    memcpy(signing_buffer + 436, raw + AMIIBO_OFFSET_MODEL_INFO, AMIIBO_MODEL_INFO_SIZE);
    memcpy(signing_buffer + 448, raw + AMIIBO_OFFSET_KEYGEN_SALT, AMIIBO_KEYGEN_SALT_SIZE);

    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(
        md_info, tag_key->hmacKey, sizeof(tag_key->hmacKey), signing_buffer + 428, 52, tag_hash);

    memcpy(signing_buffer + 396, tag_hash, AMIIBO_HASH_SIZE);

    mbedtls_md_hmac(
        md_info, data_key->hmacKey, sizeof(data_key->hmacKey), signing_buffer + 1, 479, data_hash);

    free(signing_buffer);

    return RFIDX_OK;
}

RfidxStatus amiibo_validate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data) {
    if(!tag_key || !data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    uint8_t tag_hash[AMIIBO_HASH_SIZE];
    uint8_t data_hash[AMIIBO_HASH_SIZE];

    RfidxStatus status =
        amiibo_generate_signature(tag_key, data_key, tag_data, tag_hash, data_hash);
    if(status != RFIDX_OK) {
        return status;
    }

    const uint8_t* raw = amiibo_bytes_const(tag_data);
    if(memcmp(tag_hash, raw + AMIIBO_OFFSET_TAG_HASH, AMIIBO_HASH_SIZE) != 0) {
        return RFIDX_AMIIBO_HMAC_VALIDATION_ERROR;
    }
    if(memcmp(data_hash, raw + AMIIBO_OFFSET_DATA_HASH, AMIIBO_HASH_SIZE) != 0) {
        return RFIDX_AMIIBO_HMAC_VALIDATION_ERROR;
    }

    return RFIDX_OK;
}

RfidxStatus amiibo_sign_payload(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    MfUltralightData* tag_data) {
    if(!tag_key || !data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    uint8_t tag_hash[AMIIBO_HASH_SIZE];
    uint8_t data_hash[AMIIBO_HASH_SIZE];

    RfidxStatus status =
        amiibo_generate_signature(tag_key, data_key, tag_data, tag_hash, data_hash);
    if(status != RFIDX_OK) {
        return status;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    memcpy(raw + AMIIBO_OFFSET_TAG_HASH, tag_hash, AMIIBO_HASH_SIZE);
    memcpy(raw + AMIIBO_OFFSET_DATA_HASH, data_hash, AMIIBO_HASH_SIZE);

    return RFIDX_OK;
}

RfidxStatus amiibo_format_dump(MfUltralightData* tag_data, Ntag21xMetadataHeader* header) {
    if(!tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);

    raw[9] = 0x48;
    raw[10] = 0x0F;
    raw[11] = 0xE0;

    static const uint8_t capability_defaults[4] = {0xF1, 0x10, 0xFF, 0xEE};
    memcpy(raw + AMIIBO_OFFSET_CAPABILITY, capability_defaults, sizeof(capability_defaults));

    raw[AMIIBO_OFFSET_FIXED_A5] = 0xA5;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 0] = 0x01;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 1] = 0x00;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 2] = 0x0F;
    raw[AMIIBO_OFFSET_RESERVED] = 0xBD;

    static const uint8_t cfg0[4] = {0x00, 0x00, 0x00, 0x04};
    static const uint8_t cfg1[4] = {0x5F, 0x00, 0x00, 0x00};
    memcpy(raw + AMIIBO_OFFSET_CONFIG, cfg0, sizeof(cfg0));
    memcpy(raw + AMIIBO_OFFSET_CONFIG + 4, cfg1, sizeof(cfg1));

    uint8_t password[4];
    amiibo_calculate_password(raw, password);
    memcpy(raw + AMIIBO_OFFSET_CONFIG + 8, password, sizeof(password));
    raw[AMIIBO_OFFSET_CONFIG + 12] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 13] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 14] = 0x00;
    raw[AMIIBO_OFFSET_CONFIG + 15] = 0x00;
    memset(raw + AMIIBO_OFFSET_AUTH_MAGIC, 0, AMIIBO_AUTH_MAGIC_SIZE);

    amiibo_apply_ntag215_metadata(tag_data);

    if(header) {
        memset(header, 0, sizeof(*header));
        static const uint8_t version[8] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
        memcpy(header->version, version, sizeof(version));
        header->memory_max = 134;
    }

    return RFIDX_OK;
}

RfidxStatus amiibo_change_uid(MfUltralightData* tag_data) {
    if(!tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    RfidxStatus status = amiibo_randomize_uid(raw);
    if(status != RFIDX_OK) {
        return status;
    }

    amiibo_finalize_uid_update(tag_data);

    return RFIDX_OK;
}

RfidxStatus amiibo_set_uid(MfUltralightData* tag_data, const uint8_t* uid, size_t uid_len) {
    if(!tag_data || !uid || uid_len < 7) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    amiibo_apply_uid_bytes(raw, uid);
    amiibo_finalize_uid_update(tag_data);

    return RFIDX_OK;
}

RfidxStatus amiibo_generate(
    const uint8_t* uuid,
    MfUltralightData* tag_data,
    Ntag21xMetadataHeader* header) {
    if(!uuid || !tag_data || !header) {
        return RFIDX_ARGUMENT_ERROR;
    }

    mf_ultralight_reset(tag_data);
    tag_data->type = MfUltralightTypeNTAG215;
    tag_data->pages_total = AMIIBO_PAGE_COUNT;
    tag_data->pages_read = AMIIBO_PAGE_COUNT;

    uint8_t* raw = amiibo_bytes(tag_data);
    memset(raw, 0, AMIIBO_TOTAL_BYTES);

    furi_hal_random_fill_buf(raw + AMIIBO_OFFSET_KEYGEN_SALT, AMIIBO_KEYGEN_SALT_SIZE);
    memset(raw + AMIIBO_OFFSET_MODEL_INFO, 0, AMIIBO_MODEL_INFO_SIZE);
    memcpy(raw + AMIIBO_OFFSET_MODEL_INFO, uuid, 8);

    RfidxStatus status = amiibo_randomize_uid(raw);
    if(status != RFIDX_OK) {
        return status;
    }

    status = amiibo_format_dump(tag_data, header);
    if(status != RFIDX_OK) {
        return status;
    }

    amiibo_configure_rf_interface(tag_data);

    return RFIDX_OK;
}

RfidxStatus amiibo_prepare_blank_tag(MfUltralightData* tag_data) {
    if(!tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }

    mf_ultralight_reset(tag_data);
    tag_data->type = MfUltralightTypeNTAG215;
    tag_data->pages_total = AMIIBO_PAGE_COUNT;
    tag_data->pages_read = AMIIBO_PAGE_COUNT;

    uint8_t* raw = amiibo_bytes(tag_data);
    memset(raw, 0, AMIIBO_TOTAL_BYTES);

    raw[9] = 0x48;
    static const uint8_t capability_defaults[4] = {0xE1, 0x10, 0x3E, 0x00};
    memcpy(raw + AMIIBO_OFFSET_CAPABILITY, capability_defaults, sizeof(capability_defaults));
    static const uint8_t otp_defaults[4] = {0x03, 0x00, 0xFE, 0x00};
    memcpy(tag_data->page[4].data, otp_defaults, sizeof(otp_defaults));
    static const uint8_t dynamic_lock_defaults[4] = {0x00, 0x00, 0x00, 0xBD};
    size_t dynamic_lock_page = tag_data->pages_total - 5;
    memcpy(
        tag_data->page[dynamic_lock_page].data,
        dynamic_lock_defaults,
        sizeof(dynamic_lock_defaults));
    raw[AMIIBO_OFFSET_RESERVED] = 0x00;

    RfidxStatus status = amiibo_randomize_uid(raw);
    if(status != RFIDX_OK) {
        return status;
    }

    MfUltralightConfigPages* config =
        (MfUltralightConfigPages*)(raw + AMIIBO_OFFSET_CONFIG);
    memset(config, 0, sizeof(*config));
    config->mirror.value = 0x04;
    config->rfui1 = 0x00;
    config->mirror_page = 0x00;
    config->auth0 = 0xFF;
    config->access.value = 0x00;
    config->vctid = 0x05;
    memset(config->rfui2, 0x00, sizeof(config->rfui2));
    memset(config->password.data, 0xFF, sizeof(config->password.data));
    memset(config->pack.data, 0x00, sizeof(config->pack.data));
    memset(config->rfui3, 0x00, sizeof(config->rfui3));

    size_t password_page = tag_data->pages_total - 2;
    size_t pack_page = tag_data->pages_total - 1;
    memset(tag_data->page[password_page].data, 0xFF, MF_ULTRALIGHT_PAGE_SIZE);
    memset(tag_data->page[pack_page].data, 0x00, MF_ULTRALIGHT_PAGE_SIZE);

    amiibo_configure_rf_interface(tag_data);

    return RFIDX_OK;
}
