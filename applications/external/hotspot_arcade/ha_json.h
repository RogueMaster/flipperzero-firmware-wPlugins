#pragma once
#include <furi.h>

// Minimal flat-JSON readers for the small EVENT/ROUND_RESULT/manifest payloads.
// Not a general parser; good for our controlled, shallow objects.

static inline const char* ha_json_find(const char* s, const char* key) {
    size_t klen = strlen(key);
    for(const char* p = s; *p; p++) {
        if(p[0] != '"') continue;
        if(strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            const char* q = p + 1 + klen + 1;
            while(*q == ' ')
                q++;
            if(*q != ':') continue;
            q++;
            while(*q == ' ')
                q++;
            return q;
        }
    }
    return NULL;
}

static inline bool ha_json_str(const char* s, const char* key, char* out, size_t n) {
    out[0] = '\0';
    const char* q = ha_json_find(s, key);
    if(!q || *q != '"') return false;
    q++;
    size_t i = 0;
    while(*q && *q != '"' && i < n - 1) {
        if(*q == '\\' && q[1]) {
            q++;
            out[i++] = *q;
        } else {
            out[i++] = *q;
        }
        q++;
    }
    out[i] = '\0';
    return true;
}

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
    while(*q >= '0' && *q <= '9')
        v = v * 10 + (*q++ - '0');
    *out = (int)(neg ? -v : v);
    return true;
}

static inline bool ha_json_bool(const char* s, const char* key) {
    const char* q = ha_json_find(s, key);
    return q && strncmp(q, "true", 4) == 0;
}
