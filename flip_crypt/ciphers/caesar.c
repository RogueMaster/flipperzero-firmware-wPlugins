#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "caesar.h"

char* encode_caesar(const char* plaintext, int32_t number) {
    size_t len = strlen(plaintext);
    char* output = malloc(len + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = plaintext[i];

        if (c >= 'a' && c <= 'z') {
            output[i] = 'a' + ((c - 'a' + number) % 26 + 26) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            output[i] = 'A' + ((c - 'A' + number) % 26 + 26) % 26;
        } else {
            output[i] = c;
        }
    }

    output[len] = '\0';
    return output;
}

char* decode_caesar(const char* ciphertext, int32_t number) {
    size_t len = strlen(ciphertext);
    char* output = malloc(len + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = ciphertext[i];

        if (c >= 'a' && c <= 'z') {
            output[i] = 'a' + ((c - 'a' - number) % 26 + 26) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            output[i] = 'A' + ((c - 'A' - number) % 26 + 26) % 26;
        } else {
            output[i] = c;
        }
    }

    output[len] = '\0';
    return output;
}