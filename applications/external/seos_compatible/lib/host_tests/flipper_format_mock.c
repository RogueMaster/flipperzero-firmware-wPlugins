/* Host stand-in for the Flipper key-value file reader.
 *
 * A key is searched for from the cursor to end of file and never before it,
 * and a miss leaves the cursor at the end. This is what makes an out-of-order
 * read lose a field, and one failed read poison every read after it.
 */
#include <lib/flipper_format/flipper_format.h>

#define FLIPPER_FORMAT_MOCK_CAPACITY 4096

struct FlipperFormat {
    char text[FLIPPER_FORMAT_MOCK_CAPACITY];
    size_t len;
    size_t cursor;
};

FlipperFormat* flipper_format_string_alloc_from(const char* contents) {
    FlipperFormat* format = calloc(1, sizeof(FlipperFormat));
    furi_check(format);

    size_t len = strlen(contents);
    furi_check(len < sizeof(format->text));
    memcpy(format->text, contents, len);
    format->len = len;
    format->cursor = 0;
    return format;
}

void flipper_format_free(FlipperFormat* format) {
    free(format);
}

size_t flipper_format_mock_cursor(const FlipperFormat* format) {
    return format->cursor;
}

bool flipper_format_mock_at_end(const FlipperFormat* format) {
    return format->cursor >= format->len;
}

bool flipper_format_rewind(FlipperFormat* format) {
    format->cursor = 0;
    return true;
}

/* End of the line starting at `start`. */
static size_t line_end(const FlipperFormat* format, size_t start) {
    size_t end = start;
    while(end < format->len && format->text[end] != '\n') end++;
    return end;
}

/* Finds `key` at the start of a line at or after the cursor.
 *
 * On a hit, the cursor moves past that line and the value is reported. On a
 * miss, the cursor is left at end of file, matching the device. */
static bool find_key(FlipperFormat* format, const char* key, const char** value, size_t* value_len) {
    size_t key_len = strlen(key);
    size_t at = format->cursor;

    while(at < format->len) {
        size_t end = line_end(format, at);
        size_t len = end - at;

        if(len > key_len + 1 && strncmp(format->text + at, key, key_len) == 0 &&
           format->text[at + key_len] == ':') {
            size_t value_at = at + key_len + 1;
            while(value_at < end && format->text[value_at] == ' ') value_at++;

            *value = format->text + value_at;
            *value_len = end - value_at;
            format->cursor = end < format->len ? end + 1 : format->len;
            return true;
        }

        at = end < format->len ? end + 1 : format->len;
    }

    format->cursor = format->len;
    return false;
}

bool flipper_format_read_header(FlipperFormat* format, FuriString* name, uint32_t* version) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!find_key(format, "Filetype", &value, &value_len)) return false;

    char text[128];
    if(value_len >= sizeof(text)) return false;
    memcpy(text, value, value_len);
    text[value_len] = '\0';
    furi_string_set_str(name, text);

    if(!find_key(format, "Version", &value, &value_len)) return false;
    *version = (uint32_t)strtoul(value, NULL, 10);
    return true;
}

bool flipper_format_read_uint32(
    FlipperFormat* format,
    const char* key,
    uint32_t* data,
    const uint16_t count) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!find_key(format, key, &value, &value_len)) return false;

    const char* at = value;
    for(uint16_t i = 0; i < count; i++) {
        char* after = NULL;
        data[i] = (uint32_t)strtoul(at, &after, 10);
        if(after == at) return false;
        at = after;
    }
    return true;
}

bool flipper_format_read_hex(
    FlipperFormat* format,
    const char* key,
    uint8_t* data,
    const uint16_t count) {
    const char* value = NULL;
    size_t value_len = 0;
    if(!find_key(format, key, &value, &value_len)) return false;

    /* Two characters a byte, separated by single spaces. */
    size_t needed = count == 0 ? 0 : (size_t)count * 3 - 1;
    if(value_len < needed) return false;

    for(uint16_t i = 0; i < count; i++) {
        char pair[3] = {value[i * 3], value[i * 3 + 1], '\0'};
        char* after = NULL;
        unsigned long byte = strtoul(pair, &after, 16);
        if(after != pair + 2) return false;
        data[i] = (uint8_t)byte;
    }
    return true;
}
