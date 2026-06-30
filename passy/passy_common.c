#include "passy_common.h"

#define PASSY_WORKER_MAX_BUFFER_SIZE 128

#define TAG "PassyCommon"

static char asn1_log[PASSY_WORKER_MAX_BUFFER_SIZE];
int passy_print_struct_callback(const void* buffer, size_t size, void* app_key) {
    if(app_key) {
        char* str = (char*)app_key;
        size_t next = strlen(str);
        strncpy(str + next, buffer, size);
    } else {
        uint8_t next = strlen(asn1_log);
        strncpy(asn1_log + next, buffer, size);
    }
    return 0;
}

void passy_log_bitbuffer(char* tag, char* prefix, BitBuffer* buffer) {
    furi_assert(buffer);

    size_t length = bit_buffer_get_size_bytes(buffer);
    const uint8_t* data = bit_buffer_get_data(buffer);

    char display[PASSY_WORKER_MAX_BUFFER_SIZE * 2 + 1];

    size_t limit = MIN((size_t)PASSY_WORKER_MAX_BUFFER_SIZE, length);
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < limit; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", data[i]);
    }
    if(prefix) {
        FURI_LOG_D(tag, "%s %d: %s", prefix, length, display);
    } else {
        FURI_LOG_D(tag, "Buffer %d: %s", length, display);
    }
}

void passy_log_buffer(char* tag, char* prefix, const uint8_t* buffer, size_t buffer_len) {
    char display[PASSY_WORKER_MAX_BUFFER_SIZE * 2 + 1];

    size_t limit = MIN((size_t)PASSY_WORKER_MAX_BUFFER_SIZE, buffer_len);
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < limit; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", buffer[i]);
    }
    if(prefix) {
        FURI_LOG_D(tag, "%s %d: %s", prefix, limit, display);
    } else {
        FURI_LOG_D(tag, "Buffer %d: %s", limit, display);
    }
}

// ISO/IEC 9797-1 MAC Algorithm 3
void passy_mac(uint8_t* key, uint8_t* data, size_t data_length, uint8_t* mac, bool prepadded) {
    size_t block_count = data_length / 8;
    uint8_t y[8];
    memset(y, 0, sizeof(y));

    for(size_t i = 0; i < block_count; i++) {
        uint8_t* x = data + (i * 8);
        //passy_log_buffer(TAG, "x", x, 8);

        uint8_t iv[8];
        memcpy(iv, y, sizeof(y));
        mbedtls_des_context ctx;
        mbedtls_des_init(&ctx);
        mbedtls_des_setkey_enc(&ctx, key); // uses 8 bytes
        mbedtls_des_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, 8, iv, x, y);
        mbedtls_des_free(&ctx);
        //passy_log_buffer(TAG, "y", y, 8);
    }

    mbedtls_des_context ctx;

    if(!prepadded) {
        // last block
        uint8_t last_block[8] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        uint8_t iv[8];
        memcpy(iv, y, sizeof(y));
        mbedtls_des_init(&ctx);
        mbedtls_des_setkey_enc(&ctx, key); // uses 8 bytes
        mbedtls_des_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT, 8, iv, last_block, y);
        //passy_log_buffer(TAG, "y", y, 8);
    }

    uint8_t b[8];
    mbedtls_des_init(&ctx);
    mbedtls_des_setkey_dec(&ctx, key + 8); // uses 8 bytes
    mbedtls_des_crypt_ecb(&ctx, y, b);
    //passy_log_buffer(TAG, "b", b, 8);

    mbedtls_des_init(&ctx);
    mbedtls_des_setkey_enc(&ctx, key); // uses 8 bytes
    mbedtls_des_crypt_ecb(&ctx, b, mac);
    //passy_log_buffer(TAG, "mac", mac, 8);

    mbedtls_des_free(&ctx);
}

// https://en.wikipedia.org/wiki/Machine-readable_passport
char passy_checksum(char* str) {
    size_t sum = 0;
    uint8_t weight_map[] = {7, 3, 1};
    for(uint8_t i = 0; i < strlen(str); i++) {
        uint8_t value;
        char c = toupper(str[i]);
        uint8_t weight = weight_map[(i % 3)];
        if(c >= '0' && c <= '9') {
            value = c - '0';
        } else if(c >= 'A' && c <= 'Z') {
            value = c - 'A' + 10;
        } else if(c == '<') {
            value = 0;
        } else {
            FURI_LOG_E(TAG, "Invalid character %02x", c);
            return -1;
        }
        sum += value * weight;
    }
    return 0x30 + (sum % 10);
}

size_t passy_furi_string_filename_safe(FuriString* string) {
    FuriString* safe = furi_string_alloc();

    size_t len = furi_string_size(string);
    for(size_t ri = 0; ri < len; ++ri) {
        char c = furi_string_get_char(string, ri);

        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || '/' || '.') {
            furi_string_push_back(safe, c);
        }
    }

    furi_string_set(string, safe);
    furi_string_free(safe);
    return furi_string_size(string);
}
