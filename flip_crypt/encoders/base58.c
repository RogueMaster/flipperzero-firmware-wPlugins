#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "base58.h"

static const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

char* base58_encode(const char* input) {
    size_t len = strlen(input);
    const uint8_t* bytes = (const uint8_t*)input;

    size_t result_size = len * 2;
    char* result = calloc(result_size + 1, 1);
    if (!result) return "memory allocation failed, try again";

    uint8_t* temp = calloc(len * 2, 1);
    if (!temp) {
        free(result);
        return NULL;
    }

    size_t i, high = 0, zcount = 0;
    while (zcount < len && bytes[zcount] == 0) {
        ++zcount;
    }

    memcpy(temp, bytes, len);

    while (len > 0 && high < result_size) {
        int carry = 0;
        size_t newlen = 0;

        for (i = 0; i < len; i++) {
            carry = carry * 256 + temp[i];
            if (newlen || carry / 58) {
                temp[newlen++] = carry / 58;
            }
            carry %= 58;
        }

        result[high++] = BASE58_ALPHABET[carry];
        len = newlen;
    }

    while (zcount--) {
        result[high++] = BASE58_ALPHABET[0];
    }

    // Reverse the result
    for (i = 0; i < high / 2; i++) {
        char tmp = result[i];
        result[i] = result[high - 1 - i];
        result[high - 1 - i] = tmp;
    }

    result[high] = '\0';
    free(temp);
    return result;
}

int base58_char_value(char c) {
    const char* p = strchr(BASE58_ALPHABET, c);
    return (p ? (int)(p - BASE58_ALPHABET) : -1);
}

char* base58_decode(const char* input) {
    size_t len = strlen(input);
    uint8_t* temp = calloc(len, 1);
    if (!temp) return "memory allocation failed, try again";

    size_t i, j, zcount = 0, high = 0;

    while (zcount < len && input[zcount] == BASE58_ALPHABET[0]) {
        ++zcount;
    }

    for (i = 0; i < len; i++) {
        int carry = base58_char_value(input[i]);
        if (carry < 0) {
            free(temp);
            return "memory allocation failed, try again";
        }

        for (j = 0; j < high; j++) {
            carry += 58 * temp[j];
            temp[j] = carry % 256;
            carry /= 256;
        }

        while (carry) {
            temp[high++] = carry % 256;
            carry /= 256;
        }
    }

    size_t total_len = zcount + high;
    char* result = calloc(total_len + 1, 1);
    if (!result) {
        free(temp);
        return "memory allocation failed, try again";
    }

    for (i = 0; i < zcount; i++) {
        result[i] = 0x00;
    }

    for (i = 0; i < high; i++) {
        result[zcount + i] = temp[high - 1 - i];
    }

    result[total_len] = '\0';
    free(temp);
    return result;
}
