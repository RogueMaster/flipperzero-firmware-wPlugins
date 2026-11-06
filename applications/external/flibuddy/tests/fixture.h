/* Loading protocol fixtures in host-compiled tests.
 *
 * PROTOCOL_DIR is the absolute path of the repository's protocol/ directory,
 * passed in by the Makefile. Fixture files are pretty-printed; the Flipper
 * emits compact JSON, so tests compare after fixture_compact_json().
 */
#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifndef PROTOCOL_DIR
#error "PROTOCOL_DIR must point at the repository's protocol/ directory"
#endif

/* Reads protocol/<relative> into buf as a NUL-terminated string. Returns
 * false when the file is missing or does not fit. */
static inline bool fixture_read(const char* relative, char* buf, size_t cap) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", PROTOCOL_DIR, relative);
    FILE* f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "cannot open fixture %s\n", path);
        return false;
    }
    size_t n = fread(buf, 1, cap, f);
    bool fits = n < cap && feof(f);
    fclose(f);
    if(!fits) {
        fprintf(stderr, "fixture %s does not fit in %zu bytes\n", path, cap);
        return false;
    }
    buf[n] = '\0';
    return true;
}

/* Removes JSON whitespace outside string literals, in place. */
static inline void fixture_compact_json(char* json) {
    char* out = json;
    bool in_string = false;
    bool escaped = false;
    for(const char* in = json; *in; in++) {
        char c = *in;
        if(in_string) {
            if(escaped)
                escaped = false;
            else if(c == '\\')
                escaped = true;
            else if(c == '"')
                in_string = false;
        } else if(c == '"') {
            in_string = true;
        } else if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        *out++ = c;
    }
    *out = '\0';
}

/* Reads a fixture and compacts it. */
static inline bool fixture_read_compact(const char* relative, char* buf, size_t cap) {
    if(!fixture_read(relative, buf, cap)) return false;
    fixture_compact_json(buf);
    return true;
}
