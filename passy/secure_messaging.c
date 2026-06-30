#include "secure_messaging.h"

#define TAG "SecureMessaging"

uint8_t padding[16] =
    {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void passy_secure_messaging_adjust_parity(uint8_t key[16]) {
    for(size_t i = 0; i < 16; i++) {
        // Set the parity bit to 1 if the number of 1 bits is even
        for(size_t j = 0; j < 8; j++) {
            if((key[i] >> j) & 0x01) {
                key[i] ^= 0x01;
            }
        }
        key[i] ^= 0x01;
    }
}

void passy_secure_messaging_key_diversification(
    uint8_t input[20],
    size_t input_len,
    uint8_t* output) {
    uint8_t sha[20];
    mbedtls_sha1_context ctx;
    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx, input, input_len);
    mbedtls_sha1_finish(&ctx, sha);

    memcpy(output, sha, 16);
    passy_secure_messaging_adjust_parity(output);
}

SecureMessaging* passy_secure_messaging_alloc(
    uint8_t* passport_number,
    uint8_t* date_of_birth,
    uint8_t* date_of_expiry) {
    SecureMessaging* secure_messaging = malloc(sizeof(SecureMessaging));
    memset(secure_messaging, 0, sizeof(SecureMessaging));
    mbedtls_sha1_context ctx;

    memset(secure_messaging->rndIFD, 0x00, sizeof(secure_messaging->rndIFD));
    memset(secure_messaging->Kifd, 0x00, sizeof(secure_messaging->Kifd));

    uint8_t mrz[SECURE_MESSAGING_MAX_SIZE];
    size_t mrz_index = 0;
    strncpy((char*)mrz, (char*)passport_number, 12);
    mrz_index += strlen((char*)passport_number);
    strncpy((char*)mrz + mrz_index, (char*)date_of_birth, 8);
    mrz_index += strlen((char*)date_of_birth);
    strncpy((char*)mrz + mrz_index, (char*)date_of_expiry, 8);
    mrz_index += strlen((char*)date_of_expiry);
    FURI_LOG_D(TAG, "secure_messaging_alloc mrz %s", mrz);

    uint8_t sha[20];

    mbedtls_sha1_init(&ctx);
    mbedtls_sha1_starts(&ctx);
    mbedtls_sha1_update(&ctx, mrz, mrz_index);
    mbedtls_sha1_finish(&ctx, sha);

    uint8_t D[20];
    memset(D, 0, sizeof(D));
    memcpy(D, sha, 16);

    D[19] = 0x01;
    passy_secure_messaging_key_diversification(D, sizeof(D), secure_messaging->KENC);

    D[19] = 0x02;
    passy_secure_messaging_key_diversification(D, sizeof(D), secure_messaging->KMAC);

    mbedtls_sha1_free(&ctx);
    return secure_messaging;
}

void passy_secure_messaging_free(SecureMessaging* secure_messaging) {
    furi_assert(secure_messaging);
    // Nothing to free;
    free(secure_messaging);
}

void passy_secure_messaging_calculate_session_keys(SecureMessaging* secure_messaging) {
    uint8_t Kseed[16];
    for(size_t i = 0; i < sizeof(Kseed); i++) {
        Kseed[i] = secure_messaging->Kifd[i] ^ secure_messaging->Kicc[i];
    }

    uint8_t D[20];

    memset(D, 0, sizeof(D));
    memcpy(D, Kseed, sizeof(Kseed));

    D[19] = 0x01;
    passy_secure_messaging_key_diversification(D, sizeof(D), secure_messaging->KSenc);

    D[19] = 0x02;
    passy_secure_messaging_key_diversification(D, sizeof(D), secure_messaging->KSmac);
}

void passy_secure_messaging_increment_context(SecureMessaging* secure_messaging) {
    uint8_t* context = secure_messaging->SSC;
    size_t context_len = sizeof(secure_messaging->SSC);
    do {
    } while(++context[--context_len] == 0 && context_len > 0);
}

void passy_secure_messaging_wrap_apdu(SecureMessaging* secure_messaging, BitBuffer* tx_buffer) {
    furi_assert(secure_messaging);
    passy_secure_messaging_increment_context(secure_messaging);

    // Read tx_buffer to wrap the apdu in secure messaging
    size_t message_len = bit_buffer_get_size_bytes(tx_buffer);
    const uint8_t* message = bit_buffer_get_data(tx_buffer);

    uint8_t payload_length = 0;
    bool has_le = false;
    if(message_len == 5) { // APDU with no payload and Le
        has_le = true;
    } else {
        payload_length = message[4];
    }

    uint8_t cmd_header[8];
    memset(cmd_header, 0, sizeof(cmd_header));
    memcpy(cmd_header, message, 4);
    cmd_header[0] |= 0x0c;
    cmd_header[4] = 0x80;

    uint8_t D087[3 + 8];
    if(payload_length > 0) {
        const uint8_t* payload = message + 5;
        if(payload_length > 7) {
            FURI_LOG_W(TAG, "secure_messaging_wrap_apdu payload length too large to handle");
            return;
        }
        uint8_t padded_payload[8];
        memset(padded_payload, 0, sizeof(padded_payload));
        memcpy(padded_payload, payload, payload_length);
        padded_payload[payload_length] = 0x80;

        uint8_t encrypted_payload[8];
        uint8_t iv[8];
        memset(iv, 0, sizeof(iv));
        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_enc(&ctx, secure_messaging->KSenc);
        mbedtls_des3_crypt_cbc(
            &ctx,
            MBEDTLS_DES_ENCRYPT,
            sizeof(padded_payload),
            iv,
            padded_payload,
            encrypted_payload);
        mbedtls_des3_free(&ctx);

        memset(D087, 0, sizeof(D087));
        D087[0] = 0x87;
        D087[1] = 1 + sizeof(encrypted_payload);
        D087[2] = 0x01; // TODO: look into the meaning of this
        memcpy(D087 + 3, encrypted_payload, sizeof(encrypted_payload));
    }

    uint8_t D097[3];
    memset(D097, 0, sizeof(D097));
    if(has_le) {
        D097[0] = 0x97;
        D097[1] = 0x01;
        D097[2] = message[message_len - 1];
    }

    uint8_t M[8 + 3 + 8 /* + 2*/];
    uint8_t M_index = 0;
    memset(M, 0, sizeof(M));
    memcpy(M, cmd_header, sizeof(cmd_header));
    M_index += sizeof(cmd_header);

    if(payload_length > 0) {
        memcpy(M + M_index, D087, sizeof(D087));
        M_index += sizeof(D087);
    }

    if(has_le) {
        memcpy(M + M_index, D097, sizeof(D097));
        M_index += sizeof(D097);
    }

    uint8_t N[32];
    uint8_t N_index = 0;
    memset(N, 0, sizeof(N));
    memcpy(N, secure_messaging->SSC, sizeof(secure_messaging->SSC));
    N_index += sizeof(secure_messaging->SSC);
    memcpy(N + N_index, M, M_index);
    N_index += M_index;
    N[N_index++] = 0x80;
    // Align to 8 bytes
    uint8_t block_count = (N_index + 7) / 8;
    N_index = block_count * 8;

    uint8_t mac[8];
    passy_mac(secure_messaging->KSmac, N, N_index, mac, true);

    uint8_t D08E[2 + 8];
    memset(D08E, 0, sizeof(D08E));
    D08E[0] = 0x8E;
    D08E[1] = sizeof(mac);
    memcpy(D08E + 2, mac, sizeof(mac));

    bit_buffer_reset(tx_buffer);

    bit_buffer_append_bytes(tx_buffer, cmd_header, 4);

    uint8_t protected_payload_length = 0;
    protected_payload_length += sizeof(D08E);

    if(payload_length > 0) {
        protected_payload_length += sizeof(D087);
    }
    if(has_le) {
        protected_payload_length += sizeof(D097);
    }

    // Lc
    bit_buffer_append_byte(tx_buffer, protected_payload_length);

    if(payload_length > 0) {
        bit_buffer_append_bytes(tx_buffer, D087, sizeof(D087));
    }

    if(has_le) {
        bit_buffer_append_bytes(tx_buffer, D097, sizeof(D097));
    }
    bit_buffer_append_bytes(tx_buffer, D08E, sizeof(D08E));
    bit_buffer_append_byte(tx_buffer, 0x00); // Le
}

void passy_secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    passy_secure_messaging_increment_context(secure_messaging);

    size_t length = bit_buffer_get_size_bytes(rx_buffer);
    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    uint8_t status_word[2];
    uint8_t* mac = NULL;
    uint8_t* encrypted = NULL;
    uint8_t encrypted_len = 0;

    // Look for mac
    uint8_t i = 0;
    do {
        uint8_t type = data[i++];
        uint8_t len = data[i++];
        switch(type) {
        case 0x87:
            // Encrypted data always starts with a 0x01
            encrypted = (uint8_t*)data + i + 1;
            encrypted_len = len - 1;
            break;
        case 0x8E:
            mac = (uint8_t*)data + i;
            break;
        case 0x99:
            status_word[0] = data[i + 0];
            status_word[1] = data[i + 1];
            break;
        default:
            FURI_LOG_W(TAG, "Unknown type %02x", type);
            break;
        }
        i += len;
    } while(i < length - 2);

    if(mac) {
        uint8_t K[SECURE_MESSAGING_MAX_SIZE];
        memset(K, 0, sizeof(K));
        uint8_t K_index = 0;
        memcpy(K, secure_messaging->SSC, sizeof(secure_messaging->SSC));
        K_index += sizeof(secure_messaging->SSC);
        if(encrypted) {
            K[K_index++] = 0x87;
            K[K_index++] = encrypted_len + 1;
            K[K_index++] = 0x01;
            memcpy(K + K_index, encrypted, encrypted_len);
            K_index += encrypted_len;
        }

        // Assume the status word is always present
        K[K_index++] = 0x99;
        K[K_index++] = 0x02;
        memcpy(K + K_index, status_word, 2);
        K_index += 2;
        K[K_index++] = 0x80;
        // Align to 8 bytes
        uint8_t block_count = (K_index + 7) / 8;
        K_index = block_count * 8;
        uint8_t calculated_mac[8];
        passy_mac(secure_messaging->KSmac, K, K_index, calculated_mac, true);
        if(memcmp(mac, calculated_mac, sizeof(calculated_mac)) != 0) {
            FURI_LOG_W(TAG, "Invalid MAC");
            return;
        }
    }

    uint8_t decrypted[SECURE_MESSAGING_MAX_SIZE];
    uint8_t decrypted_len = encrypted_len;
    if(encrypted) {
        if(encrypted_len > sizeof(decrypted)) {
            FURI_LOG_W(TAG, "secure_messaging_unwrap_rapdu encrypted length too large to handle");
            return;
        }
        uint8_t iv[8];
        memset(iv, 0, sizeof(iv));
        mbedtls_des3_context ctx;
        mbedtls_des3_init(&ctx);
        mbedtls_des3_set2key_dec(&ctx, secure_messaging->KSenc);
        mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT, encrypted_len, iv, encrypted, decrypted);
        mbedtls_des3_free(&ctx);

        // Remove padding
        do {
        } while(decrypted[--decrypted_len] == 0 && decrypted_len > 0);
    }

    // Don't reset until after data has been decrypted
    bit_buffer_reset(rx_buffer);
    if(encrypted) {
        bit_buffer_append_bytes(rx_buffer, decrypted, decrypted_len);
    }

    bit_buffer_append_bytes(rx_buffer, status_word, 2);
}
