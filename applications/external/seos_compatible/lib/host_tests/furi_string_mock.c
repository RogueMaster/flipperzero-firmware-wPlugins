/* Host stand-in for FuriString.
 *
 * Covers only what file parsing needs: hold a header name and compare it.
 */
#include <furi.h>

#define FURI_STRING_MOCK_CAPACITY 128

struct FuriString {
    char text[FURI_STRING_MOCK_CAPACITY];
};

FuriString* furi_string_alloc(void) {
    FuriString* string = calloc(1, sizeof(FuriString));
    furi_check(string);
    return string;
}

void furi_string_free(FuriString* string) {
    free(string);
}

void furi_string_set_str(FuriString* string, const char* str) {
    snprintf(string->text, sizeof(string->text), "%s", str);
}

const char* furi_string_get_cstr(const FuriString* string) {
    return string->text;
}

int furi_string_cmp_str(const FuriString* string, const char* str) {
    return strcmp(string->text, str);
}

bool furi_string_empty(const FuriString* string) {
    return string->text[0] == '\0';
}
