#include "adfgvx.h"

#include <furi.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ADFGVX_GRID_SIZE    6
#define ADFGVX_ALPHABET_LEN 36

static const char kAdfgvxLabels[ADFGVX_GRID_SIZE] = {'A', 'D', 'F', 'G', 'V', 'X'};
static const char kAdfgvxGrid[ADFGVX_ALPHABET_LEN] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static bool adfgvx_char_allowed(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
}

// Uppercases input and strips anything that isn't a letter or digit
static char* adfgvx_sanitize(const char* input, size_t* out_len) {
    size_t len = strlen(input);
    char* out = malloc(len + 1);
    furi_assert(out);
    size_t j = 0;
    for(size_t i = 0; i < len; i++) {
        char c = (char)toupper((unsigned char)input[i]);
        if(adfgvx_char_allowed(c)) {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    *out_len = j;
    return out;
}

static void adfgvx_locate(char c, int* row, int* col) {
    for(int i = 0; i < ADFGVX_ALPHABET_LEN; i++) {
        if(kAdfgvxGrid[i] == c) {
            *row = i / ADFGVX_GRID_SIZE;
            *col = i % ADFGVX_GRID_SIZE;
            return;
        }
    }
    *row = 0;
    *col = 0;
}

static char adfgvx_lookup(int row, int col) {
    return kAdfgvxGrid[row * ADFGVX_GRID_SIZE + col];
}

// Stable alphabetical read-order of the key's columns, ties broken by
// original left-to-right position. E.g. key "ZEBRA" -> [4,1,3,2,0]
// (positions of A,E,B,R,Z within "ZEBRA").
static int* adfgvx_key_order(const char* key, size_t m) {
    int* order = malloc(m * sizeof(int));
    furi_assert(order);
    bool* used = calloc(m, sizeof(bool));
    furi_assert(used);

    for(size_t pick = 0; pick < m; pick++) {
        int best = -1;
        for(size_t i = 0; i < m; i++) {
            if(used[i]) continue;
            if(best == -1 || toupper((unsigned char)key[i]) < toupper((unsigned char)key[best])) {
                best = (int)i;
            }
        }
        order[pick] = best;
        used[best] = true;
    }
    free(used);
    return order;
}

char* adfgvx_encrypt(const char* input, const char* key) {
    furi_assert(input);
    furi_assert(key);

    size_t m = strlen(key);
    if(m == 0) return strdup(input);

    size_t plain_len;
    char* clean = adfgvx_sanitize(input, &plain_len);
    if(plain_len == 0) {
        free(clean);
        return strdup("");
    }

    // Every plaintext char becomes 2 letters (row, col)
    size_t frac_len = plain_len * 2;
    char* frac = malloc(frac_len + 1);
    furi_assert(frac);
    for(size_t i = 0; i < plain_len; i++) {
        int row, col;
        adfgvx_locate(clean[i], &row, &col);
        frac[i * 2] = kAdfgvxLabels[row];
        frac[i * 2 + 1] = kAdfgvxLabels[col];
    }
    frac[frac_len] = '\0';
    free(clean);

    // Irregular columnar transposition keyed by `key`.
    size_t num_rows = (frac_len + m - 1) / m;
    size_t remainder = frac_len % m; // columns [0, remainder) get one extra row

    int* order = adfgvx_key_order(key, m);

    char* out = malloc(frac_len + 1);
    furi_assert(out);
    size_t out_pos = 0;

    for(size_t k = 0; k < m; k++) {
        int col = order[k];
        size_t col_len = (remainder == 0 || (size_t)col < remainder) ? num_rows : num_rows - 1;
        for(size_t r = 0; r < col_len; r++) {
            out[out_pos++] = frac[r * m + (size_t)col];
        }
    }
    out[out_pos] = '\0';

    free(frac);
    free(order);
    return out;
}

char* adfgvx_decrypt(const char* input, const char* key) {
    furi_assert(input);
    furi_assert(key);

    size_t m = strlen(key);
    if(m == 0) return strdup(input);

    size_t cipher_len;
    char* clean = adfgvx_sanitize(input, &cipher_len);
    if(cipher_len == 0) {
        free(clean);
        return strdup("");
    }

    size_t num_rows = (cipher_len + m - 1) / m;
    size_t remainder = cipher_len % m;

    int* order = adfgvx_key_order(key, m);

    // Rebuild the fractionation grid
    char* frac = malloc(cipher_len + 1);
    furi_assert(frac);
    memset(frac, 0, cipher_len + 1);

    size_t in_pos = 0;
    for(size_t k = 0; k < m; k++) {
        int col = order[k];
        size_t col_len = (remainder == 0 || (size_t)col < remainder) ? num_rows : num_rows - 1;
        for(size_t r = 0; r < col_len; r++) {
            frac[r * m + (size_t)col] = clean[in_pos++];
        }
    }
    free(clean);
    free(order);

    // Undo fractionation
    size_t plain_len = cipher_len / 2;
    char* out = malloc(plain_len + 1);
    furi_assert(out);
    for(size_t i = 0; i < plain_len; i++) {
        char row_c = frac[i * 2];
        char col_c = frac[i * 2 + 1];
        int row = 0, col = 0;
        for(int j = 0; j < ADFGVX_GRID_SIZE; j++) {
            if(kAdfgvxLabels[j] == row_c) row = j;
            if(kAdfgvxLabels[j] == col_c) col = j;
        }
        out[i] = adfgvx_lookup(row, col);
    }
    out[plain_len] = '\0';

    free(frac);
    return out;
}
