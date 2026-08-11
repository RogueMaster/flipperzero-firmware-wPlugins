#include "meshcore_json.h"

#include <string.h>

/* Depth tracking is what makes "top-level only" true rather than aspirational:
 * a key inside a nested object must not be mistaken for one of ours. */
static const char* meshcore_json_find_key(const char* json, const char* key) {
    size_t key_len = strlen(key);
    int depth = 0;
    bool in_string = false;
    bool escaped = false;

    for(const char* p = json; *p; p++) {
        if(escaped) {
            escaped = false;
            continue;
        }
        if(in_string) {
            if(*p == '\\') {
                escaped = true;
            } else if(*p == '"') {
                in_string = false;
            }
            continue;
        }

        if(*p == '"') {
            /* A string at depth 1 that is followed by a colon is a key of the
             * object we care about. */
            if(depth == 1 && strncmp(p + 1, key, key_len) == 0 && p[1 + key_len] == '"') {
                const char* after = p + 2 + key_len;
                while(*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;
                if(*after == ':') return after + 1;
            }
            in_string = true;
            continue;
        }

        if(*p == '{' || *p == '[') depth++;
        if(*p == '}' || *p == ']') depth--;
    }

    return NULL;
}

bool meshcore_json_get(const char* json, const char* key, char* out, size_t cap) {
    if(!json || !key || !out || cap == 0) return false;

    const char* value = meshcore_json_find_key(json, key);
    if(!value) return false;

    while(*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') value++;

    size_t written = 0;

    if(*value == '"') {
        value++;
        while(*value && *value != '"') {
            if(*value == '\\' && value[1]) value++; /* keep it simple: take the
                                                     * escaped character as-is */
            if(written + 1 >= cap) return false;
            out[written++] = *value++;
        }
        if(*value != '"') return false; /* unterminated */
    } else {
        /* Number, boolean or null: runs until a delimiter. */
        while(*value && *value != ',' && *value != '}' && *value != ']' && *value != '\n' &&
              *value != '\r') {
            if(written + 1 >= cap) return false;
            out[written++] = *value++;
        }
        /* Trailing whitespace is not part of the value. */
        while(written > 0 && (out[written - 1] == ' ' || out[written - 1] == '\t')) written--;
        if(written == 0) return false;
    }

    out[written] = '\0';
    return true;
}

bool meshcore_parse_scaled(const char* text, uint32_t scale, uint32_t* out) {
    if(!text || !out || scale == 0) return false;

    const char* p = text;
    while(*p == ' ') p++;
    if(*p == '+') p++;
    if(*p == '-') return false; /* every field this is used for is unsigned */
    if(*p == '\0') return false;

    uint64_t whole = 0;
    bool any_digit = false;
    while(*p >= '0' && *p <= '9') {
        whole = whole * 10 + (uint64_t)(*p - '0');
        if(whole > 0xFFFFFFFFull) return false;
        any_digit = true;
        p++;
    }

    uint64_t value = whole * scale;

    if(*p == '.') {
        p++;
        /* Consume the fraction digit by digit, folding each into the result at
         * its own weight. Digits past the scale's precision are dropped, which
         * is the same truncation the reference client does — the wire has no
         * room for them. */
        uint32_t weight = scale;
        while(*p >= '0' && *p <= '9') {
            any_digit = true;
            weight /= 10;
            if(weight > 0) value += (uint64_t)(*p - '0') * weight;
            p++;
        }
    }

    while(*p == ' ') p++;
    if(*p != '\0' || !any_digit) return false;
    if(value > 0xFFFFFFFFull) return false;

    *out = (uint32_t)value;
    return true;
}

bool meshcore_json_get_uint(const char* json, const char* key, uint32_t* out) {
    char buffer[24];
    if(!meshcore_json_get(json, key, buffer, sizeof(buffer))) return false;
    return meshcore_parse_scaled(buffer, 1, out);
}

bool meshcore_json_get_scaled(const char* json, const char* key, uint32_t scale, uint32_t* out) {
    char buffer[24];
    if(!meshcore_json_get(json, key, buffer, sizeof(buffer))) return false;
    return meshcore_parse_scaled(buffer, scale, out);
}

bool meshcore_json_get_bool(const char* json, const char* key, bool* out) {
    char buffer[8];
    if(!meshcore_json_get(json, key, buffer, sizeof(buffer))) return false;
    if(strcmp(buffer, "true") == 0) {
        *out = true;
        return true;
    }
    if(strcmp(buffer, "false") == 0) {
        *out = false;
        return true;
    }
    return false;
}
