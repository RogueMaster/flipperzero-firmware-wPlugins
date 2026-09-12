#include "base16.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* string_to_base16(const char* input) {
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
