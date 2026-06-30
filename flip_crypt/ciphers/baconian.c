#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "baconian.h"

const char* baconian_table[26] = {
    "AAAAA", "AAAAB", "AAABA", "AAABB", "AABAA", "AABAB",
    "AABBA", "AABBB", "ABAAA", "ABAAB", "ABABA", "ABABB",
    "ABBAA", "ABBAB", "ABBBA", "ABBBB", "BAAAA", "BAAAB",
    "BAABA", "BAABB", "BABAA", "BABAB", "BABBA", "BABBB",
    "BBAAA", "BBAAB"
};

char* baconian_encrypt(const char* text) {
    size_t len = strlen(text);
    char* encoded = (char*)malloc(len * 5 + 1);
    if (!encoded) return "memory allocation failed, please try again";

    size_t pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (isalpha((unsigned char)text[i])) {
            char upper = toupper((unsigned char)text[i]);
            int index = upper - 'A';
            if (index >= 0 && index < 26) {
                const char* code = baconian_table[index];
                for (int j = 0; j < 5; j++) {
                    encoded[pos++] = code[j];
                }
            }
        } else {
            encoded[pos++] = text[i]; // Preserve non-letters as-is
        }
    }

    encoded[pos] = '\0';
    return encoded;
}

char* baconian_decrypt(const char* encoded) {
    size_t len = strlen(encoded);
    char* decoded = (char*)malloc(len / 5 + 2);
    if (!decoded) return "memory allocation failed, please try again";

    size_t i = 0;
    size_t pos = 0;

    while (i + 4 < len) {
        if (!isalpha((unsigned char)encoded[i])) {
            i++; // skip non-alpha
            continue;
        }

        char chunk[6] = {0};
        for (int j = 0; j < 5 && encoded[i + j] != '\0'; j++) {
            chunk[j] = toupper((unsigned char)encoded[i + j]);
        }

        int matched = 0;
        for (int j = 0; j < 26; j++) {
            if (strncmp(chunk, baconian_table[j], 5) == 0) {
                decoded[pos++] = 'A' + j;
                matched = 1;
                break;
            }
        }

        if (!matched) {
            decoded[pos++] = '?'; // Unrecognized chunk
        }

        i += 5;
    }

    decoded[pos] = '\0';
    return decoded;
}
