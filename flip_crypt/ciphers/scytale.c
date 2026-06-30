#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

char* scytale_encrypt(const char* plaintext, int32_t key) {
    if (key <= 1) return strdup("key <= 1, encryption failed");
    if (!plaintext) return strdup("text input does not exist, try again");

    int len = strlen(plaintext);
    int cols = (int)ceil((double)len / key); // columns
    char* ciphertext = malloc(len + 1);
    if (!ciphertext) return strdup("memory allocation failed, try again");

    int index = 0;
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < key; j++) {
            int pos = j * cols + i;
            if (pos < len) {
                ciphertext[index++] = plaintext[pos];
            }
        }
    }

    ciphertext[index] = '\0';
    return ciphertext;
}


char* scytale_decrypt(const char* ciphertext, int32_t key) {
    if (key <= 1) return strdup("key <= 1, decryption failed");
    if (!ciphertext) return strdup("text input does not exist, try again");

    int len = strlen(ciphertext);
    int cols = (int)ceil((double)len / key);
    char* plaintext = malloc(len + 1);
    if (!plaintext) return strdup("memory allocation failed, try again");

    int index = 0;
    for (int j = 0; j < key; j++) {
        for (int i = 0; i < cols; i++) {
            int pos = i * key + j;
            if (pos < len) {
                plaintext[index++] = ciphertext[pos];
            }
        }
    }

    plaintext[index] = '\0';
    return plaintext;
}
