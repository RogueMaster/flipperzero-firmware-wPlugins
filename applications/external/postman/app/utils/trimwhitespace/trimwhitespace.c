#include <furi.h>
#include <furi_hal.h>

void trim_whitespace(char* str) {
    char* end;

    // Trim leading space
    while(isspace((unsigned char)*str))
        str++;

    if(*str == 0) // All spaces?
        return;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator
    *(end + 1) = '\0';
}
