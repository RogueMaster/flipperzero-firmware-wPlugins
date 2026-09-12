#include "base16.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* base16_encode(const char* input) {
    if (input == NULL) return "no input provided";

    size_t len = strlen(input);
    
    char* hex_str = (char*)malloc((len * 2) + 1);
    if (hex_str == NULL) return "malloc failed"; 

    for (size_t i = 0; i < len; i++) {
        // %02x formats the character as a 2-digit lowercase hex number
        // (unsigned char) cast prevents sign extension issues
        sprintf(&hex_str[i * 2], "%02x", (unsigned char)input[i]);
    }

    hex_str[len * 2] = '\0'; 

    return hex_str;
}

#include "base16.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char* base16_decode(const char* input, size_t* out_len) {
    if (input == NULL) return "no input provided";

    size_t len = strlen(input);

    // A valid hex string must have an even number of characters
    if (len % 2 != 0) return "invalid input length";

    char* decoded = (char*)malloc((len / 2) + 1);
    if (decoded == NULL) return "malloc failed";

    for (size_t i = 0; i < len / 2; i++) {
        char high = input[i * 2];
        char low  = input[i * 2 + 1];

        if (!isxdigit((unsigned char)high) || !isxdigit((unsigned char)low)) {
            free(decoded);
            return "invalid hex character";
        }

        // sscanf reads two hex chars into one byte
        unsigned int byte;
        sscanf(&input[i * 2], "%2x", &byte);
        decoded[i] = (char)byte;
    }

    decoded[len / 2] = '\0';

    if (out_len != NULL) {
        *out_len = len / 2;
    }

    return decoded;
}
