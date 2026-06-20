#include "seos_common.h"

char* seos_file_header = "Flipper Seos Credential";
uint32_t seos_file_version = 1;

void seos_log_buffer(char* TAG, char* prefix, uint8_t* buffer, size_t buffer_len) {
    char display[SEOS_WORKER_MAX_BUFFER_SIZE * 2 + 1];

    size_t limit = MIN((size_t)SEOS_WORKER_MAX_BUFFER_SIZE, buffer_len);
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < limit; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", buffer[i]);
    }
    if(prefix) {
        FURI_LOG_D(TAG, "%s %d: %s", prefix, limit, display);
    } else {
        FURI_LOG_D(TAG, "Buffer %d: %s", limit, display);
    }
}

void seos_log_bitbuffer(char* TAG, char* prefix, BitBuffer* buffer) {
    furi_assert(buffer);

    size_t length = bit_buffer_get_size_bytes(buffer);
    const uint8_t* data = bit_buffer_get_data(buffer);

    char display[SEOS_WORKER_MAX_BUFFER_SIZE * 2 + 1];

    size_t limit = MIN((size_t)SEOS_WORKER_MAX_BUFFER_SIZE, length);
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < limit; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", data[i]);
    }
    if(prefix) {
        FURI_LOG_D(TAG, "%s %d: %s", prefix, length, display);
    } else {
        FURI_LOG_D(TAG, "Buffer %d: %s", length, display);
    }
}

void seos_worker_diversify_key(
    uint8_t master_key_value[16],
    uint8_t* diversifier,
    size_t diversifier_len,
    uint8_t* adf_oid,
    size_t adf_oid_len,
    uint8_t algo_id1,
    uint8_t algo_id2,
    uint8_t keyId,
    bool is_encryption,
    uint8_t* div_key) {
    char* TAG = "SeosCommon";
    // 0000000000000000000000 04 00 0080 01 0907 01 2B0601040181E4380101020118010102 3D50AD518CD820
    size_t index = 0;
    uint8_t buffer[128];
    memset(buffer, 0, sizeof(buffer));
    index += 11;
    buffer[index++] = is_encryption ? 0x04 : 0x06;
    index++; // separation
    index++; // 0x00 that goes with 0x80 to indicate 128bit key
    buffer[index++] = 0x80;
    buffer[index++] = 0x01; // i
    buffer[index++] = algo_id1;
    buffer[index++] = algo_id2;
    buffer[index++] = keyId;
    memcpy(buffer + index, adf_oid, adf_oid_len);
    index += adf_oid_len;
    memcpy(buffer + index, diversifier, diversifier_len);
    index += diversifier_len;

    aes_cmac(master_key_value, 16, buffer, index, div_key);

    char display[33];
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < 16; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", div_key[i]);
    }
    FURI_LOG_I(TAG, "Diversified %s key: %s", is_encryption ? "Encrypt" : "Mac", display);
}

void seos_worker_aes_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 128);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, length, iv, encrypted, clear);
    mbedtls_aes_free(&ctx);
}

void seos_worker_des_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));
    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    mbedtls_des3_set2key_dec(&ctx, key);
    mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, length, iv, encrypted, clear);
    mbedtls_des3_free(&ctx);
}

void seos_worker_aes_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, length, iv, clear, encrypted);
    mbedtls_aes_free(&ctx);
}

void seos_worker_des_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    uint8_t iv[8];
    memset(iv, 0, sizeof(iv));
    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    mbedtls_des3_set2key_enc(&ctx, key);
    mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, length, iv, clear, encrypted);
    mbedtls_des3_free(&ctx);
}
