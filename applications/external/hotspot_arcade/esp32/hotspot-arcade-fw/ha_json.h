// Minimal JSON helpers for the tiny WebSocket / UART messages. Dependency-free
// (no ArduinoJson): incoming messages are small and flat, so a shallow scan is
// enough. Not a general JSON parser; good for `{"t":"answer","c":2}`-shaped data.
#pragma once
#include <Arduino.h>

// Find `"key"` then the following `:` and return a pointer just past the colon
// (skipping spaces), or nullptr. Only scans the top level well enough for our
// flat objects (values are strings/ints/arrays we control).
static inline const char* ha_json_find(const char* s, const char* key) {
    size_t klen = strlen(key);
    for(const char* p = s; *p; p++) {
        if(p[0] != '"') continue;
        if(strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            const char* q = p + 1 + klen + 1;
            while(*q == ' ') q++;
            if(*q != ':') continue;
            q++;
            while(*q == ' ') q++;
            return q;
        }
    }
    return nullptr;
}

// Read a string value for `key` into out (NUL-terminated, basic \" \\ unescape).
static inline bool ha_json_str(const char* s, const char* key, char* out, size_t n) {
    out[0] = '\0';
    const char* q = ha_json_find(s, key);
    if(!q || *q != '"') return false;
    q++;
    size_t i = 0;
    while(*q && *q != '"' && i < n - 1) {
        if(*q == '\\' && q[1]) {
            q++;
            char c = *q;
            if(c == 'n')
                c = '\n';
            else if(c == 't')
                c = '\t';
            out[i++] = c;
        } else {
            out[i++] = *q;
        }
        q++;
    }
    out[i] = '\0';
    return true;
}

// Read an integer value for `key`. Returns false if absent/non-numeric.
static inline bool ha_json_int(const char* s, const char* key, int* out) {
    const char* q = ha_json_find(s, key);
    if(!q) return false;
    bool neg = false;
    if(*q == '-') {
        neg = true;
        q++;
    }
    if(*q < '0' || *q > '9') return false;
    long v = 0;
    while(*q >= '0' && *q <= '9') v = v * 10 + (*q++ - '0');
    *out = (int)(neg ? -v : v);
    return true;
}

// Escape a string for embedding in JSON output.
static inline String ha_json_escape(const char* s) {
    String out;
    out.reserve(strlen(s) + 4);
    for(const char* p = s; *p; p++) {
        char c = *p;
        if(c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if(c == '\n') {
            out += "\\n";
        } else if((unsigned char)c < 0x20) {
            // drop other control chars
        } else {
            out += c;
        }
    }
    return out;
}
