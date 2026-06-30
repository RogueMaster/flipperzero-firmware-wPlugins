#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "railfence.h"

char* rail_fence_encrypt(const char* text, int rails) {
    if (rails <= 1 || text == NULL) return "improper input, try again";

    int len = strlen(text);
    char** rail = malloc(rails * sizeof(char*));
    int* pos = calloc(rails, sizeof(int));

    // Allocate memory for each rail
    for (int i = 0; i < rails; i++) {
        rail[i] = malloc((len + 1) * sizeof(char));
        rail[i][0] = '\0'; // initialize as empty string
    }

    int row = 0;
    int direction = 1; // 1 = down, -1 = up

    for (int i = 0; i < len; i++) {
        rail[row][pos[row]] = text[i];
        pos[row]++;

        row += direction;
        if (row == rails - 1) direction = -1;
        else if (row == 0) direction = 1;
    }

    // Concatenate all rails
    char* result = malloc(len + 1);
    int idx = 0;

    for (int i = 0; i < rails; i++) {
        for (int j = 0; j < pos[i]; j++) {
            result[idx++] = rail[i][j];
        }
        free(rail[i]);
    }
    free(rail);
    free(pos);

    result[idx] = '\0';
    return result;
}

char* rail_fence_decrypt(const char* cipher, int rails) {
    if (rails <= 1 || cipher == NULL) return "improper input, try again";

    int len = strlen(cipher);
    char* result = malloc(len + 1);
    char** fence = malloc(rails * sizeof(char*));

    for (int i = 0; i < rails; i++) {
        fence[i] = calloc(len, sizeof(char));
    }

    // Mark the zigzag pattern with '*'
    int row = 0, direction = 1;
    for (int i = 0; i < len; i++) {
        fence[row][i] = '*';
        row += direction;

        if (row == rails - 1) direction = -1;
        else if (row == 0) direction = 1;
    }

    // Fill in the pattern with ciphertext
    int idx = 0;
    for (int r = 0; r < rails; r++) {
        for (int c = 0; c < len; c++) {
            if (fence[r][c] == '*' && idx < len) {
                fence[r][c] = cipher[idx++];
            }
        }
    }

    // Read the zigzag pattern to build the plaintext
    row = 0;
    direction = 1;
    for (int i = 0; i < len; i++) {
        result[i] = fence[row][i];
        row += direction;

        if (row == rails - 1) direction = -1;
        else if (row == 0) direction = 1;
    }

    result[len] = '\0';

    for (int i = 0; i < rails; i++) {
        free(fence[i]);
    }
    free(fence);

    return result;
}
