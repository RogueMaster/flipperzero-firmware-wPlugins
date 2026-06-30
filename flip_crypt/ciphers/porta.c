#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "porta.h"

static int number_to_index(char letter) {
    if (letter >= 'a' && letter <= 'z') {
        return letter - 'a';
    } else if (letter >= 'A' && letter <= 'Z') {
        return letter - 'A';
    } else {
        return -1; // not a letter
    }
}

static inline int porta_map_index(int p_index, int k_index) {
    int j = (k_index >= 0 ? k_index / 2 : 0); // pair index 0..12
    if (p_index < 13) {
        // A–M map into N–Z, shifted by j within the half
        return 13 + ((p_index + j) % 13);
    } else {
        // N–Z map into A–M, shifted by -j within the half
        return ((p_index - 13 - j) % 13 + 13) % 13; // ensure 0..12
    }
}

char* porta_encrypt_and_decrypt(const char* plaintext, const char* keyword) {
    size_t n = strlen(plaintext);
    size_t klen = strlen(keyword);

    char* output = malloc(n + 1);
    if (!output) return "memory allocation failed, try again";

    size_t ki = 0; // advance key only when we process a letter

    for (size_t i = 0; i < n; i++) {
        char p = plaintext[i];
        int p_index = number_to_index(p);

        if (p_index >= 0) {
            // pull next alphabetic key character
            int k_index;
            do {
                char k = keyword[ki % klen];
                k_index = number_to_index(k);
                ki++; // consume key position only when we hit a plaintext letter
            } while (k_index < 0); // skip any non-letters in key

            int c_index = porta_map_index(p_index, k_index);
            char c = (char)('A' + c_index);
            output[i] = islower((unsigned char)p) ? (char)tolower((unsigned char)c) : c;
        } else {
            output[i] = p; // non-letter
        }
    }

    output[n] = '\0';
    return output;
}