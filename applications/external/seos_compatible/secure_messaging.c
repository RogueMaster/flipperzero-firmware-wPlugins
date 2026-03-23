#include "secure_messaging.h"

#define TAG "SecureMessaging"

static uint8_t padding[16] =
    {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

SecureMessaging* secure_messaging_alloc(AuthParameters* params) {
    SecureMessaging* secure_messaging = malloc(sizeof(SecureMessaging));
    memset(secure_messaging, 0, sizeof(SecureMessaging));

    secure_messaging->cipher = params->cipher;
    if(params->cipher == AES_128_CBC) {
        memcpy(secure_messaging->aesContext, params->rndICC, 8);
        memcpy(secure_messaging->aesContext + 8, params->UID, 8);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        memcpy(secure_messaging->desContext, params->rndICC, 4);
        memcpy(secure_messaging->desContext + 4, params->UID, 4);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    size_t index = 0;
    uint8_t buffer[38];
    memset(buffer, 0, sizeof(buffer));
    index += 4; // skip 4 bytes where iteration will be put
    memcpy(buffer + index, params->cNonce, 8);
    index += 8;
    memcpy(buffer + index, params->rNonce, 8);
    index += 8;
    buffer[index++] = params->cipher;
    buffer[index++] = params->cipher;
    memcpy(buffer + index, params->rndICC, 8);
    index += 8;
    memcpy(buffer + index, params->UID, 8);
    index += 8;

    size_t iterations = 1;
    size_t unit = 0;
    if(params->hash == SHA1) {
        unit = 160 / 8;
    } else if(params->hash == SHA256) {
        unit = 256 / 8;
    }
    // FURI_LOG_D(TAG, "secure_messaging_alloc hash %d unit %d", hash, unit);

    // More than enough space for the hash
    uint8_t accumulator[64];
    memset(accumulator, 0, sizeof(accumulator));
    for(size_t i = 0; i < 32; i += unit) {
        buffer[3] = iterations++;
        if(params->hash == SHA1) {
            mbedtls_sha1_context ctx;
            mbedtls_sha1_init(&ctx);
            mbedtls_sha1_starts(&ctx);
            mbedtls_sha1_update(&ctx, buffer, index);
            mbedtls_sha1_finish(&ctx, accumulator + i);
            mbedtls_sha1_free(&ctx);
        } else if(params->hash == SHA256) {
            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            mbedtls_sha256_starts(&ctx, 0);
            mbedtls_sha256_update(&ctx, buffer, index);
            mbedtls_sha256_finish(&ctx, accumulator + i);
            mbedtls_sha256_free(&ctx);
        } else {
            FURI_LOG_W(TAG, "Could not match hash algorithm");
        }
    }

    memcpy(secure_messaging->PrivacyKey, accumulator, 16);
    memcpy(secure_messaging->CMACKey, accumulator + 16, 16);

    return secure_messaging;
}

void secure_messaging_free(SecureMessaging* secure_messaging) {
    furi_assert(secure_messaging);
    // Nothing to free;
    free(secure_messaging);
}

void secure_messaging_increment_context(SecureMessaging* secure_messaging) {
    uint8_t* context = NULL;
    size_t context_len = 0;
    if(secure_messaging->cipher == AES_128_CBC) {
        context = secure_messaging->aesContext;
        context_len = sizeof(secure_messaging->aesContext);
    } else if(secure_messaging->cipher == TWO_KEY_3DES_CBC_MODE) {
        context = secure_messaging->desContext;
        context_len = sizeof(secure_messaging->desContext);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
        return;
    }
    do {
    } while(++context[--context_len] == 0 && context_len > 0);
}

void secure_messaging_wrap_apdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint8_t* apdu_header,
    size_t apdu_header_len,
    BitBuffer* tx_buffer) {
    secure_messaging_increment_context(secure_messaging);
    uint8_t cipher = secure_messaging->cipher;

    if(message_len > SECURE_MESSAGING_MAX_SIZE) {
        FURI_LOG_W(TAG, "Message too long to wrap");
        return;
    }

    uint8_t clear[SECURE_MESSAGING_MAX_SIZE];
    memset(clear, 0, sizeof(clear));
    memcpy(clear, message, message_len);
    clear[message_len] = 0x80;
    uint8_t block_size = cipher == AES_128_CBC ? 16 : 8;
    uint8_t block_count = (message_len + block_size) / block_size;
    size_t clear_len = block_count * block_size;

    uint8_t encrypted[SECURE_MESSAGING_MAX_SIZE];
    if(cipher == AES_128_CBC) {
        seos_worker_aes_encrypt(secure_messaging->PrivacyKey, clear_len, clear, encrypted);
    } else if(cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_encrypt(secure_messaging->PrivacyKey, clear_len, clear, encrypted);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    uint8_t encrypted_prefix[] = {0x85, clear_len};
    uint8_t no_something[] = {0x97, 0x00};

    uint8_t cmac[16];
    BitBuffer* cmacInput = bit_buffer_alloc(256);
    bit_buffer_reset(cmacInput);
    if(cipher == AES_128_CBC) {
        uint8_t* context = secure_messaging->aesContext;
        bit_buffer_append_bytes(cmacInput, context, block_size);
        bit_buffer_append_bytes(cmacInput, apdu_header, apdu_header_len);
        bit_buffer_append_bytes(cmacInput, padding, block_size - apdu_header_len);
        bit_buffer_append_bytes(cmacInput, encrypted_prefix, sizeof(encrypted_prefix));
        bit_buffer_append_bytes(cmacInput, encrypted, clear_len);
        bit_buffer_append_bytes(cmacInput, no_something, sizeof(no_something));
        // Need to pad [encrypted_prefix, encrypted, no_something], and encrypted is by definition block_size units,so we just need to pad to align the other 4.
        bit_buffer_append_bytes(cmacInput, padding, block_size - 4);

        aes_cmac(
            secure_messaging->CMACKey,
            sizeof(secure_messaging->CMACKey),
            (uint8_t*)bit_buffer_get_data(cmacInput),
            bit_buffer_get_size_bytes(cmacInput),
            cmac);
    } else if(cipher == TWO_KEY_3DES_CBC_MODE) {
        uint8_t* context = secure_messaging->desContext;
        bit_buffer_append_bytes(cmacInput, context, block_size);
        bit_buffer_append_bytes(cmacInput, apdu_header, apdu_header_len);
        bit_buffer_append_bytes(cmacInput, padding, block_size - apdu_header_len);
        bit_buffer_append_bytes(cmacInput, encrypted_prefix, sizeof(encrypted_prefix));
        bit_buffer_append_bytes(cmacInput, encrypted, clear_len);
        bit_buffer_append_bytes(cmacInput, no_something, sizeof(no_something));
        // Need to pad [encrypted_prefix, encrypted, no_something], and encrypted is by definition block_size units,so we just need to pad to align the other 4.
        bit_buffer_append_bytes(cmacInput, padding, block_size - 4);

        des_cmac(
            secure_messaging->CMACKey,
            sizeof(secure_messaging->CMACKey),
            (uint8_t*)bit_buffer_get_data(cmacInput),
            bit_buffer_get_size_bytes(cmacInput),
            cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    bit_buffer_free(cmacInput);

    uint8_t apdu_len[] = {2 + clear_len + 2 + 2 + 8};
    uint8_t cmac_prefix[] = {0x8e, 0x08};
    uint8_t Le[] = {0x00};

    bit_buffer_reset(tx_buffer);
    bit_buffer_append_bytes(tx_buffer, apdu_header, apdu_header_len);
    bit_buffer_append_bytes(tx_buffer, apdu_len, sizeof(apdu_len));
    bit_buffer_append_bytes(tx_buffer, encrypted_prefix, sizeof(encrypted_prefix));
    bit_buffer_append_bytes(tx_buffer, encrypted, clear_len);
    bit_buffer_append_bytes(tx_buffer, no_something, sizeof(no_something));
    bit_buffer_append_bytes(tx_buffer, cmac_prefix, sizeof(cmac_prefix));
    bit_buffer_append_bytes(tx_buffer, cmac, 8);
    bit_buffer_append_bytes(tx_buffer, Le, sizeof(Le));
}

void secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    secure_messaging_increment_context(secure_messaging);
    // 8540 <encrypted> 99029000 8e08 <cmac>

    size_t encrypted_len = bit_buffer_get_byte(rx_buffer, 1);
    const uint8_t* encrypted = bit_buffer_get_data(rx_buffer) + 2;
    // const uint8_t *cmac = bit_buffer_get_data(rx_buffer) + 2 + encrypted_len + 6;

    uint8_t clear[SECURE_MESSAGING_MAX_SIZE];
    memset(clear, 0, sizeof(clear));

    // TODO: check cmac
    if(secure_messaging->cipher == AES_128_CBC) {
        seos_worker_aes_decrypt(secure_messaging->PrivacyKey, encrypted_len, encrypted, clear);
    } else if(secure_messaging->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_decrypt(secure_messaging->PrivacyKey, encrypted_len, encrypted, clear);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    size_t clear_len = encrypted_len;
    do {
    } while(clear[--clear_len] == 0 && clear_len > 0);
    bit_buffer_reset(rx_buffer);
    bit_buffer_append_bytes(rx_buffer, clear, clear_len);
}

// Assumes it is an iso14443a-4 and doesn't have framing bytes
/*
0ccb3fff
16
  8508
    4088b37ca72bc7ae
  9700
  8e08
    85345f0f5c44b980
00
*/
void secure_messaging_unwrap_apdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    secure_messaging_increment_context(secure_messaging);

    // TODO: check cmac
    const uint8_t* encrypted = bit_buffer_get_data(rx_buffer) + 7;
    size_t encrypted_len = bit_buffer_get_byte(rx_buffer, 6);

    if(encrypted_len > SECURE_MESSAGING_MAX_SIZE) {
        FURI_LOG_W(TAG, "message too large (%d) to unwrap", encrypted_len);
        return;
    }

    uint8_t clear[SECURE_MESSAGING_MAX_SIZE];
    memset(clear, 0, sizeof(clear));
    if(secure_messaging->cipher == AES_128_CBC) {
        seos_worker_aes_decrypt(secure_messaging->PrivacyKey, encrypted_len, encrypted, clear);
    } else if(secure_messaging->cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_decrypt(secure_messaging->PrivacyKey, encrypted_len, encrypted, clear);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    do {
    } while(clear[--encrypted_len] == 0 && encrypted_len > 0);
    bit_buffer_reset(rx_buffer);
    bit_buffer_append_bytes(rx_buffer, clear, encrypted_len);
}

void secure_messaging_wrap_rapdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    BitBuffer* tx_buffer) {
    secure_messaging_increment_context(secure_messaging);
    uint8_t cipher = secure_messaging->cipher;

    if(message_len > SECURE_MESSAGING_MAX_SIZE) {
        FURI_LOG_W(TAG, "Message too long to wrap");
        return;
    }

    // Copy into clear so we can pad it.
    uint8_t clear[SECURE_MESSAGING_MAX_SIZE];
    memset(clear, 0, sizeof(clear));
    memcpy(clear, message, message_len);
    clear[message_len] = 0x80;
    uint8_t block_size = cipher == AES_128_CBC ? 16 : 8;
    uint8_t block_count = (message_len + block_size - 1) / block_size;
    size_t clear_len = block_count * block_size;

    uint8_t encrypted[SECURE_MESSAGING_MAX_SIZE];
    if(cipher == AES_128_CBC) {
        seos_worker_aes_encrypt(secure_messaging->PrivacyKey, clear_len, clear, encrypted);
    } else if(cipher == TWO_KEY_3DES_CBC_MODE) {
        seos_worker_des_encrypt(secure_messaging->PrivacyKey, clear_len, clear, encrypted);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }

    uint8_t encrypted_prefix[] = {0x85, clear_len};
    uint8_t cmac_prefix[] = {0x8e, 0x08};
    uint8_t something[] = {0x99, 0x02, 0x90, 0x00};

    uint8_t cmac[16];
    BitBuffer* cmacInput = bit_buffer_alloc(256);
    bit_buffer_reset(cmacInput);
    if(cipher == AES_128_CBC) {
        uint8_t* context = secure_messaging->aesContext;
        bit_buffer_append_bytes(cmacInput, context, block_size);
        bit_buffer_append_bytes(cmacInput, encrypted_prefix, sizeof(encrypted_prefix));
        bit_buffer_append_bytes(cmacInput, encrypted, clear_len);
        bit_buffer_append_bytes(cmacInput, something, sizeof(something));
        // Need to pad to multiple of block size, but context and encrypted are already block size.
        bit_buffer_append_bytes(
            cmacInput, padding, block_size - sizeof(encrypted_prefix) - sizeof(something));

        aes_cmac(
            secure_messaging->CMACKey,
            sizeof(secure_messaging->CMACKey),
            (uint8_t*)bit_buffer_get_data(cmacInput),
            bit_buffer_get_size_bytes(cmacInput),
            cmac);
    } else if(cipher == TWO_KEY_3DES_CBC_MODE) {
        uint8_t* context = secure_messaging->desContext;
        bit_buffer_append_bytes(cmacInput, context, block_size);
        bit_buffer_append_bytes(cmacInput, encrypted_prefix, sizeof(encrypted_prefix));
        bit_buffer_append_bytes(cmacInput, encrypted, clear_len);
        bit_buffer_append_bytes(cmacInput, something, sizeof(something));
        // Need to pad to multiple of block size, but context and encrypted are already block size.
        bit_buffer_append_bytes(
            cmacInput, padding, block_size - sizeof(encrypted_prefix) - sizeof(something));

        des_cmac(
            secure_messaging->CMACKey,
            sizeof(secure_messaging->CMACKey),
            (uint8_t*)bit_buffer_get_data(cmacInput),
            bit_buffer_get_size_bytes(cmacInput),
            cmac);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
    }
    bit_buffer_free(cmacInput);

    bit_buffer_append_bytes(tx_buffer, encrypted_prefix, sizeof(encrypted_prefix));
    bit_buffer_append_bytes(tx_buffer, encrypted, clear_len);
    bit_buffer_append_bytes(tx_buffer, something, sizeof(something));
    bit_buffer_append_bytes(tx_buffer, cmac_prefix, sizeof(cmac_prefix));
    bit_buffer_append_bytes(tx_buffer, cmac, 8);
    // Success (9000) is appended by common code before transmission

    /*
8540
  2b4f4e5598193e71cc94ff6dc2b24d1e9feae4182d7315b8b11a8a034670fe8c369f2a98c256dd6f4decf28277a180ea1c4ce515812abfa683e1e004bc66d757
9902
  9000
8e08
  a860944446f6d53d
9000
*/
}
