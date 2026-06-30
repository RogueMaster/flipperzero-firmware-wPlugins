#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atbash.h"

char* atbash_encrypt_or_decrypt(const char* input) {
    size_t len = strlen(input);
    char* output = (char*)malloc(len + 1);
    if (!output) return "memory allocation failed, try again";

    for (size_t i = 0; i < len; i++) {
        char c = input[i];
        if (isupper(c)) {
            output[i] = 'Z' - (c - 'A');
        } else if (islower(c)) {
            output[i] = 'z' - (c - 'a');
        } else {
            output[i] = c;
        }
    }
    output[len] = '\0';
    return output;
}
