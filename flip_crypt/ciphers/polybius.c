#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "polybius.h"

const char* polybius_square[5][5] = {
    {"A","B","C","D","E"},
    {"F","G","H","I","K"},
    {"L","M","N","O","P"},
    {"Q","R","S","T","U"},
    {"V","W","X","Y","Z"}
};

// Find coordinates of a character in the square (row and col are 1-based)
void find_coords(char c, int* row, int* col) {
    if (c == 'J') c = 'I'; // Treat I/J the same
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (polybius_square[i][j][0] == c) {
                *row = i + 1;
                *col = j + 1;
                return;
            }
        }
    }
}

char* encrypt_polybius(const char* plaintext) {
    size_t len = strlen(plaintext);
    char* ciphertext = malloc(len * 2 + 1); // Each letter -> 2 digits
    if (!ciphertext) return "memory allocation failed, try again";

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (isalpha((unsigned char)plaintext[i])) {
            char upper = toupper(plaintext[i]);
            int row, col;
            find_coords(upper, &row, &col);
            ciphertext[pos++] = '0' + row;
            ciphertext[pos++] = '0' + col;
        }
    }
    ciphertext[pos] = '\0';
    return ciphertext;
}

char* decrypt_polybius(const char* ciphertext) {
    size_t len = strlen(ciphertext);
    char* plaintext = malloc(len / 2 + 1); // Each 2 digits -> 1 letter
    if (!plaintext) return "memory allocation failed, try again";

    size_t pos = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        if (isdigit((unsigned char)ciphertext[i]) && isdigit((unsigned char)ciphertext[i + 1])) {
            int row = ciphertext[i] - '0' - 1;
            int col = ciphertext[i + 1] - '0' - 1;
            if (row >= 0 && row < 5 && col >= 0 && col < 5) {
                plaintext[pos++] = polybius_square[row][col][0];
            }
        }
    }
    plaintext[pos] = '\0';
    return plaintext;
}