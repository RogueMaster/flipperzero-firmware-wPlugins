#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int letter_to_index(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 1;
    else if (c >= 'a' && c <= 'z')
        return c - 'a' + 1;
    else
        return -1; // Not a letter
}

char index_to_letter(int index, int is_upper) {
    if (index < 1 || index > 26) return '?';
    return (is_upper ? 'A' : 'a') + (index - 1);
}

int mod_inverse(int a, int m) {
    a = a % m;
    for (int x = 1; x < m; x++) {
        if ((a * x) % m == 1)
            return x;
    }
    return -1; // No inverse
}

char* encode_affine(const char* text, uint8_t a, uint8_t b) {
    if (a % 2 == 0) return "First key must be odd";
    if (a == 13) return "First key cannot be 13";

    size_t len = strlen(text);
    char* result = malloc(len + 1);
    if (!result) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        int x = letter_to_index(c);
        if (x == -1) {
            result[i] = c; // Non-letter, so don't modify
        } else {
            int is_upper = (c >= 'A' && c <= 'Z');
            int newnum = ((a * x + b) % 26);
            if (newnum == 0) newnum = 26;
            result[i] = index_to_letter(newnum, is_upper);
        }
    }

    result[len] = '\0';
    return result;
}

char* decode_affine(const char* text, uint8_t a, uint8_t b) {
    size_t len = strlen(text);
    char* result = malloc(len + 1);
    if (!result) return "memory allocation failed, try again";

    int a_inv = mod_inverse(a, 26);
    if (a_inv == -1) {
        strcpy(result, "invalid key: a has no modular inverse\nValid a vals: 1, 3, 5, 7, 9, 11, 15, 17, 19, 21, 23, 25");
        return result;
    }

    for (size_t i = 0; i < len; i++) {
        char c = text[i];
        int x = letter_to_index(c);
        if (x == -1) {
            result[i] = c; // Keep non-letters as-is
        } else {
            int is_upper = (c >= 'A' && c <= 'Z');
            int y = x;
            int orig = (a_inv * ((y - b + 26) % 26)) % 26;
            if (orig == 0) orig = 26;
            result[i] = index_to_letter(orig, is_upper);
        }
    }

    result[len] = '\0';
    return result;
}