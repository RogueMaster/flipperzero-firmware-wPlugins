#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beaufort.h"

char* beaufort_cipher_encrypt_and_decrypt(const char* text, const char* key) {
    int len = strlen(text);
    int key_len = strlen(key);
    int key_index = 0;

    char* result = malloc(len + 1);
    if (!result) return "memory allocation failed, try again";

    for (int i = 0; i < len; i++) {
        char c = text[i];

        if (isalpha(c)) {
            char k = key[key_index % key_len];
            int shift = tolower(k) - 'a';

            if (isupper(c)) {
                result[i] = ((shift - (c - 'A') + 26) % 26) + 'A';
            } else {
                result[i] = ((shift - (c - 'a') + 26) % 26) + 'a';
            }

            key_index++;
        } else {
            result[i] = c;
        }
    }

    result[len] = '\0';
    return result;
}
