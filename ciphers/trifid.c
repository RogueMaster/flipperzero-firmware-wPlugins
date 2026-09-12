#include "trifid.h"

#include <furi.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define TRIFID_DIM 3
#define TRIFID_ALPHABET_LEN 27 // A-Z plus '.'

// Builds a 27-char keyed alphabet example keyword "SECRET" -> "SECRTABDFGHIJKLMNOPQUVWXYZ."
static void trifid_build_cube(const char* key, char* cube) {
    bool used[27] = {false}; // index 26 = '.'
    size_t pos = 0;

    if(key) {
        for(size_t i = 0; key[i] != '\0'; i++) {
            char c = (char)toupper((unsigned char)key[i]);
            int idx = (c == '.') ? 26 : (c >= 'A' && c <= 'Z' ? c - 'A' : -1);
            if(idx < 0 || used[idx]) continue;
            used[idx] = true;
            cube[pos++] = (idx == 26) ? '.' : (char)('A' + idx);
        }
    }
    for(int idx = 0; idx < TRIFID_ALPHABET_LEN && pos < TRIFID_ALPHABET_LEN; idx++) {
        if(used[idx]) continue;
        used[idx] = true;
        cube[pos++] = (idx == 26) ? '.' : (char)('A' + idx);
    }
}

static void trifid_locate(const char* cube, char c, int* layer, int* row, int* col) {
    for(int i = 0; i < TRIFID_ALPHABET_LEN; i++) {
        if(cube[i] == c) {
            *layer = i / (TRIFID_DIM * TRIFID_DIM);
            *row = (i / TRIFID_DIM) % TRIFID_DIM;
            *col = i % TRIFID_DIM;
            return;
        }
    }
    *layer = 0;
    *row = 0;
    *col = 0;
}

static char* trifid_sanitize(const char* input, size_t* out_len) {
    size_t len = strlen(input);
    char* out = malloc(len + 1);
    furi_assert(out);
    size_t j = 0;
    for(size_t i = 0; i < len; i++) {
        char c = (char)toupper((unsigned char)input[i]);
        if((c >= 'A' && c <= 'Z') || c == '.') {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

char* trifid_encrypt(const char* input, const char* key) {
    furi_assert(input);

    char cube[TRIFID_ALPHABET_LEN];
    trifid_build_cube(key, cube);

    size_t len;
    char* clean = trifid_sanitize(input, &len);
    if(len == 0) {
        free(clean);
        return strdup("");
    }

    // Collect layers, rows, cols into one array of length 3*len.
    int* combined = malloc(3 * len * sizeof(int));
    furi_assert(combined);
    for(size_t i = 0; i < len; i++) {
        int layer, row, col;
        trifid_locate(cube, clean[i], &layer, &row, &col);
        combined[i] = layer; // layers block
        combined[len + i] = row; // rows block
        combined[2 * len + i] = col; // cols block
    }
    free(clean);

    // Read the combined sequence off in triples, map back through the cube.
    char* out = malloc(len + 1);
    furi_assert(out);
    for(size_t i = 0; i < len; i++) {
        int layer = combined[i * 3];
        int row = combined[i * 3 + 1];
        int col = combined[i * 3 + 2];
        out[i] = cube[layer * TRIFID_DIM * TRIFID_DIM + row * TRIFID_DIM + col];
    }
    out[len] = '\0';

    free(combined);
    return out;
}

char* trifid_decrypt(const char* input, const char* key) {
    furi_assert(input);

    char cube[TRIFID_ALPHABET_LEN];
    trifid_build_cube(key, cube);

    size_t len;
    char* clean = trifid_sanitize(input, &len);
    if(len == 0) {
        free(clean);
        return strdup("");
    }

    // Ciphertext letters -> flat coordinate stream of length 3*len
    int* combined = malloc(3 * len * sizeof(int));
    furi_assert(combined);
    for(size_t i = 0; i < len; i++) {
        int layer, row, col;
        trifid_locate(cube, clean[i], &layer, &row, &col);
        combined[i * 3] = layer;
        combined[i * 3 + 1] = row;
        combined[i * 3 + 2] = col;
    }
    free(clean);

    // First third of the stream is layers, next third rows, last third cols
    char* out = malloc(len + 1);
    furi_assert(out);
    for(size_t i = 0; i < len; i++) {
        int layer = combined[i];
        int row = combined[len + i];
        int col = combined[2 * len + i];
        out[i] = cube[layer * TRIFID_DIM * TRIFID_DIM + row * TRIFID_DIM + col];
    }
    out[len] = '\0';

    free(combined);
    return out;
}
