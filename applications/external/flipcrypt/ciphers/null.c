#include <stdlib.h>
#include <string.h>
#include "null.h"

static const char kFillerAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
#define FILLER_ALPHABET_LEN 26

char* null_encrypt(const char* input, int32_t n) {
    if(!input || n < 1) {
        return NULL;
    }

    size_t inlen = strlen(input);
    size_t outlen = inlen * (size_t)n;

    char* out = malloc(outlen + 1);
    if(!out) {
        return NULL;
    }

    for(size_t i = 0; i < inlen; i++) {
        size_t base = i * (size_t)n;
        for(int32_t j = 0; j < n; j++) {
            if(j == n - 1) {
                out[base + j] = input[i]; // real char sliced in
            } else {
                out[base + j] = kFillerAlphabet[rand() % FILLER_ALPHABET_LEN];
            }
        }
    }
    out[outlen] = '\0';

    return out;
}

char* null_decrypt(const char* input, int32_t n) {
    if(!input || n < 1) {
        return NULL;
    }

    size_t inlen = strlen(input);
    if(inlen % (size_t)n != 0) {
        return NULL;
    }

    size_t outlen = inlen / (size_t)n;
    char* out = malloc(outlen + 1);
    if(!out) {
        return NULL;
    }

    for(size_t i = 0; i < outlen; i++) {
        out[i] = input[i * (size_t)n + (n - 1)];
    }
    out[outlen] = '\0';

    return out;
}
