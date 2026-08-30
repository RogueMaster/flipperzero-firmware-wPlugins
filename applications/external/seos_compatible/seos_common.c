#include "seos_common.h"

char* seos_file_header = "Flipper Seos Credential";
uint32_t seos_file_version = 1;

void seos_worker_random_nonce(uint8_t* nonce, size_t len) {
    furi_hal_random_fill_buf(nonce, len);
}

void seos_log_buffer(char* TAG, char* prefix, uint8_t* buffer, size_t buffer_len) {
    /* This runs on every message. Do not build the string when the log call
     * is going to throw it away. */
    if(furi_log_get_level() < FuriLogLevelDebug) return;

    size_t limit = MIN((size_t)SEOS_WORKER_MAX_BUFFER_SIZE, buffer_len);

    char display[SEOS_WORKER_MAX_BUFFER_SIZE * 2 + 1];
    uint8_to_hex_chars(buffer, (uint8_t*)display, (int)(limit * 2));
    display[limit * 2] = '\0';

    if(prefix) {
        FURI_LOG_D(TAG, "%s %d: %s", prefix, buffer_len, display);
    } else {
        FURI_LOG_D(TAG, "Buffer %d: %s", buffer_len, display);
    }
}

void seos_log_bitbuffer(char* TAG, char* prefix, BitBuffer* buffer) {
    furi_assert(buffer);
    seos_log_buffer(
        TAG, prefix, (uint8_t*)bit_buffer_get_data(buffer), bit_buffer_get_size_bytes(buffer));
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

    /* The derived key is not logged: it is key material, and the log is not
     * the place for it. */
    FURI_LOG_D(TAG, "Diversified %s key", is_encryption ? "Encrypt" : "Mac");
}

/* One CBC pass from a zero IV, for either cipher. */
static bool
    cbc(bool aes, bool encrypt, uint8_t key[16], size_t length, const uint8_t* in, uint8_t* out) {
    uint8_t iv[16];
    memset(iv, 0, sizeof(iv));
    int rtn;

    if(aes) {
        mbedtls_aes_context ctx;
        mbedtls_aes_init(&ctx);
        rtn = encrypt ? mbedtls_aes_setkey_enc(&ctx, key, 128) :
                        mbedtls_aes_setkey_dec(&ctx, key, 128);
        if(rtn == 0) {
            rtn = mbedtls_aes_crypt_cbc(
                &ctx, encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT, length, iv, in, out);
        }
        mbedtls_aes_free(&ctx);
    } else {
        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        rtn = encrypt ? mbedtls_des3_set2key_enc(&ctx, key) : mbedtls_des3_set2key_dec(&ctx, key);
        if(rtn == 0) {
            rtn = mbedtls_des3_crypt_cbc(
                &ctx, encrypt ? MBEDTLS_DES_ENCRYPT : MBEDTLS_DES_DECRYPT, length, iv, in, out);
        }
        mbedtls_des3_free(&ctx);
    }

    return rtn == 0;
}

bool seos_worker_aes_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    return cbc(true, false, key, length, encrypted, clear);
}

bool seos_worker_des_decrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    return cbc(false, false, key, length, encrypted, clear);
}

bool seos_worker_aes_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    return cbc(true, true, key, length, clear, encrypted);
}

bool seos_worker_des_encrypt(
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    return cbc(false, true, key, length, clear, encrypted);
}

size_t seos_cipher_block_size(uint8_t cipher) {
    if(cipher == AES_128_CBC) return 16;
    if(cipher == TWO_KEY_3DES_CBC_MODE) return 8;
    return 0;
}

bool seos_cipher_encrypt(
    uint8_t cipher,
    uint8_t key[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted) {
    if(cipher == AES_128_CBC) return cbc(true, true, key, length, clear, encrypted);
    if(cipher == TWO_KEY_3DES_CBC_MODE) return cbc(false, true, key, length, clear, encrypted);
    FURI_LOG_W("SeosCommon", "Cipher not matched (%d)", cipher);
    return false;
}

bool seos_cipher_decrypt(
    uint8_t cipher,
    uint8_t key[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear) {
    if(cipher == AES_128_CBC) return cbc(true, false, key, length, encrypted, clear);
    if(cipher == TWO_KEY_3DES_CBC_MODE) return cbc(false, false, key, length, encrypted, clear);
    FURI_LOG_W("SeosCommon", "Cipher not matched (%d)", cipher);
    return false;
}

bool seos_cipher_cmac(
    uint8_t cipher,
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* cmac) {
    if(cipher == AES_128_CBC) return aes_cmac(key, key_len, message, message_len, cmac);
    if(cipher == TWO_KEY_3DES_CBC_MODE) return des_cmac(key, key_len, message, message_len, cmac);
    FURI_LOG_W("SeosCommon", "Cipher not matched (%d)", cipher);
    return false;
}
