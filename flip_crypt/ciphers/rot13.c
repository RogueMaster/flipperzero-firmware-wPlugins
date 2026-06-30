#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rot13.h"

char* encrypt_rot13(const char* plaintext) {
    size_t len = strlen(plaintext);
    char* output = malloc(len + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = plaintext[i];

        if (c >= 'a' && c <= 'z') {
            output[i] = 'a' + ((c - 'a' + 13) % 26 + 26) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            output[i] = 'A' + ((c - 'A' + 13) % 26 + 26) % 26;
        } else {
            output[i] = c;
        }
    }

    output[len] = '\0';
    return output;
}

char* decrypt_rot13(const char* ciphertext) {
    size_t len = strlen(ciphertext);
    char* output = malloc(len + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = ciphertext[i];

        if (c >= 'a' && c <= 'z') {
            output[i] = 'a' + ((c - 'a' - 13) % 26 + 26) % 26;
        } else if (c >= 'A' && c <= 'Z') {
            output[i] = 'A' + ((c - 'A' - 13) % 26 + 26) % 26;
        } else {
            output[i] = c;
        }
    }

    output[len] = '\0';
    return output;
}