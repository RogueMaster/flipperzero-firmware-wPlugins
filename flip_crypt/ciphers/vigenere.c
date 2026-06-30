#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vigenere.h"

char* vigenere_encrypt(const char* text, const char* key) {
    int len = strlen(text);
    int key_len = strlen(key);
    int key_index = 0;

    char* ciphertext = malloc(len + 1);
    if (!ciphertext) return "memory allocation failed, try again";

    for (int i = 0; i < len; i++) {
        char c = text[i];

        if (isalpha(c)) {
            char k = key[key_index % key_len];
            int shift = tolower(k) - 'a';

            if (isupper(c)) {
                ciphertext[i] = ((c - 'A' + shift) % 26) + 'A';
            } else {
                ciphertext[i] = ((c - 'a' + shift) % 26) + 'a';
            }

            key_index++;
        } else {
            ciphertext[i] = c;
        }
    }

    ciphertext[len] = '\0';
    return ciphertext;
}

char* vigenere_decrypt(const char* text, const char* key) {
    int len = strlen(text);
    int key_len = strlen(key);
    int key_index = 0;

    char* plaintext = malloc(len + 1);
    if (!plaintext) return "memory allocation failed, try again";

    for (int i = 0; i < len; i++) {
        char c = text[i];

        if (isalpha(c)) {
            char k = key[key_index % key_len];
            int shift = tolower(k) - 'a';

            if (isupper(c)) {
                plaintext[i] = ((c - 'A' - shift + 26) % 26) + 'A';
            } else {
                plaintext[i] = ((c - 'a' - shift + 26) % 26) + 'a';
            }

            key_index++;
        } else {
            plaintext[i] = c;
        }
    }

    plaintext[len] = '\0';
    return plaintext;
}