#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rc4.h"

unsigned char* rc4_encrypt_and_decrypt(const char* key, const unsigned char* input, size_t inputlen) {
    unsigned char S[256];
    unsigned int i, j = 0, k, t;
    size_t keylen = strlen(key);

    unsigned char* output = malloc(inputlen);
    if (!output) return NULL;

    // KSA
    for(i = 0; i < 256; i++) {
        S[i] = i;
    }
    for(i = 0; i < 256; i++) {
        j = (j + S[i] + (unsigned char)key[i % keylen]) & 0xFF;
        unsigned char temp = S[i];
        S[i] = S[j];
        S[j] = temp;
    }

    // PRGA
    i = j = 0;
    for(k = 0; k < inputlen; k++) {
        i = (i + 1) & 0xFF;
        j = (j + S[i]) & 0xFF;
        unsigned char temp = S[i];
        S[i] = S[j];
        S[j] = temp;
        t = (S[i] + S[j]) & 0xFF;
        output[k] = input[k] ^ S[t];
    }

    return output;
}

char* rc4_to_hex(const char* data, size_t len) {
    char* hex = malloc(len * 2 + 1);
    if(!hex) return NULL;
    for(size_t i = 0; i < len; i++) {
        snprintf(hex + i * 2, 3, "%02X", (unsigned char)data[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}

unsigned char* rc4_hex_to_bytes(const char* hex, size_t* out_len) {
    size_t hex_len = strlen(hex);
    if(hex_len % 2 != 0) return NULL;

    *out_len = hex_len / 2;
    unsigned char* bytes = malloc(*out_len);
    if(!bytes) return NULL;

    for(size_t i = 0; i < *out_len; i++) {
        unsigned int byte;
        if(sscanf(hex + 2 * i, "%2X", &byte) != 1) {
            free(bytes);
            return NULL;
        }
        bytes[i] = (unsigned char)byte;
    }

    return bytes;
}
