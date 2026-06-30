#include "debug.h"
#include <stdio.h>

char debug_text[64] = "";

void set_debug_text(char* text) {
    snprintf(debug_text, sizeof(debug_text), "%s", text);
}

char* get_debug_text() {
    return debug_text;
}
