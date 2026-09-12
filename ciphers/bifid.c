#include "bifid.h"

#include <furi.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BIFID_GRID_SIZE 5
#define BIFID_ALPHABET_LEN 25 // A-Z with J folded into I

static void bifid_build_square(const char* key, char* square) {
    bool used[26] = {false};
    used['J' - 'A'] = true; // J always folds into I, never placed separately
    size_t pos = 0;

    if(key) {
        for(size_t i = 0; key[i] != '\0'; i++) {
            char c = (char)toupper((unsigned char)key[i]);
            if(c == 'J') c = 'I';
            if(c < 'A' || c > 'Z') continue;
            if(used[c - 'A']) continue;
            used[c - 'A'] = true;
            square[pos++] = c;
        }
    }
    for(char c = 'A'; c <= 'Z' && pos < BIFID_ALPHABET_LEN; c++) {
        if(used[c - 'A']) continue;
        used[c - 'A'] = true;
        square[pos++] = c;
    }
}

static void bifid_locate(const char* square, char c, int* row, int* col) {
    for(int i = 0; i < BIFID_ALPHABET_LEN; i++) {
        if(square[i] == c) {
            *row = i / BIFID_GRID_SIZE;
            *col = i % BIFID_GRID_SIZE;
            return;
        }
    }
    *row = 0;
    *col = 0;
}

// Uppercases, strips anything that isn't a letter, combines J and I
static char* bifid_sanitize(const char* input, size_t* out_len) {
    size_t len = strlen(input);
    char* out = malloc(len + 1);
    furi_assert(out);
    size_t j = 0;
    for(size_t i = 0; i < len; i++) {
        char c = (char)toupper((unsigned char)input[i]);
        if(c == 'J') c = 'I';
        if(c >= 'A' && c <= 'Z') {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

char* bifid_encrypt(const char* input, const char* key) {
    furi_assert(input);

    char square[BIFID_ALPHABET_LEN];
    bifid_build_square(key, square);

    size_t len;
    char* clean = bifid_sanitize(input, &len);
    if(len == 0) {
        free(clean);
        return strdup("");
    }

    // Collect rows then columns into one array of length 2*len
    int* rows = malloc(len * sizeof(int));
    int* cols = malloc(len * sizeof(int));
    furi_assert(rows);
    furi_assert(cols);
    for(size_t i = 0; i < len; i++) {
        bifid_locate(square, clean[i], &rows[i], &cols[i]);
    }

    int* combined = malloc(2 * len * sizeof(int));
    furi_assert(combined);
    memcpy(combined, rows, len * sizeof(int));
    memcpy(combined + len, cols, len * sizeof(int));
    free(rows);
    free(cols);

    // Read the combined sequence off in pairs, map back through the square
    char* out = malloc(len + 1);
    furi_assert(out);
    for(size_t i = 0; i < len; i++) {
        int r = combined[i * 2];
        int c = combined[i * 2 + 1];
        out[i] = square[r * BIFID_GRID_SIZE + c];
    }
    out[len] = '\0';

    free(combined);
    free(clean);
    return out;
}

char* bifid_decrypt(const char* input, const char* key) {
    furi_assert(input);

    char square[BIFID_ALPHABET_LEN];
    bifid_build_square(key, square);

    size_t len;
    char* clean = bifid_sanitize(input, &len);
    if(len == 0) {
        free(clean);
        return strdup("");
    }

    // Ciphertext letters -> flat coordinate stream of length 2*len
    int* combined = malloc(2 * len * sizeof(int));
    furi_assert(combined);
    for(size_t i = 0; i < len; i++) {
        int row, col;
        bifid_locate(square, clean[i], &row, &col);
        combined[i * 2] = row;
        combined[i * 2 + 1] = col;
    }
    free(clean);

    // First half of the stream is rows, second half is columns
    char* out = malloc(len + 1);
    furi_assert(out);
    for(size_t i = 0; i < len; i++) {
        int r = combined[i];
        int c = combined[len + i];
        out[i] = square[r * BIFID_GRID_SIZE + c];
    }
    out[len] = '\0';

    free(combined);
    return out;
}
