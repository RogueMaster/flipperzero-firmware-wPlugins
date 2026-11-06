#include "json_reader.h"

#include <limits.h>
#include <string.h>

void json_reader_init(JsonReader* r, const char* text, size_t len) {
    r->p = text;
    r->end = text + len;
    r->ok = true;
    r->depth = 0;
    r->first = 0;
}

static bool jr_fail(JsonReader* r) {
    r->ok = false;
    return false;
}

static void jr_skip_ws(JsonReader* r) {
    while(r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
        r->p++;
}

/* Next non-whitespace character, or 0 at the end of the text. */
static char jr_peek_char(JsonReader* r) {
    jr_skip_ws(r);
    return r->p < r->end ? *r->p : '\0';
}

static bool jr_literal(JsonReader* r, const char* word) {
    size_t n = strlen(word);
    if((size_t)(r->end - r->p) < n || memcmp(r->p, word, n) != 0) return jr_fail(r);
    r->p += n;
    return true;
}

static bool jr_is_digit(char c) {
    return c >= '0' && c <= '9';
}

JsonType json_peek(JsonReader* r) {
    if(!r->ok) return JsonTypeInvalid;
    char c = jr_peek_char(r);
    switch(c) {
    case '{':
        return JsonTypeObject;
    case '[':
        return JsonTypeArray;
    case '"':
        return JsonTypeString;
    case 't':
    case 'f':
        return JsonTypeBool;
    case 'n':
        return JsonTypeNull;
    default:
        return (c == '-' || jr_is_digit(c)) ? JsonTypeNumber : JsonTypeInvalid;
    }
}

static bool jr_open(JsonReader* r, char c) {
    if(!r->ok) return false;
    if(jr_peek_char(r) != c || r->depth >= JSON_READER_MAX_DEPTH) return jr_fail(r);
    r->p++;
    r->depth++;
    r->first |= (uint16_t)(1u << r->depth);
    return true;
}

/* Common part of json_object_next / json_array_next: false at `close`
 * (consumed), true when an element follows (its separator consumed). */
static bool jr_next(JsonReader* r, char close) {
    if(!r->ok || r->depth == 0) return jr_fail(r);
    char c = jr_peek_char(r);
    uint16_t bit = (uint16_t)(1u << r->depth);
    if(c == close) {
        r->p++;
        r->first &= (uint16_t)~bit;
        r->depth--;
        return false;
    }
    if(!(r->first & bit)) {
        if(c != ',') return jr_fail(r);
        r->p++;
    }
    r->first &= (uint16_t)~bit;
    return true;
}

static int jr_hex_value(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Reads the four hex digits of a \u escape. */
static bool jr_hex4(JsonReader* r, unsigned* out) {
    if(r->end - r->p < 4) return jr_fail(r);
    unsigned v = 0;
    for(int i = 0; i < 4; i++) {
        int d = jr_hex_value(r->p[i]);
        if(d < 0) return jr_fail(r);
        v = (v << 4) | (unsigned)d;
    }
    r->p += 4;
    *out = v;
    return true;
}

/* Scans a string, writing up to `cap - 1` ASCII characters to `out`. With
 * `out == NULL` it only validates. `overflow` is set when the text did not
 * fit; that is an error only for the caller to decide. */
static bool jr_string(JsonReader* r, char* out, size_t cap, bool* overflow) {
    if(!r->ok) return false;
    if(jr_peek_char(r) != '"') return jr_fail(r);
    r->p++;
    size_t n = 0;
    *overflow = false;
    for(;;) {
        if(r->p >= r->end) return jr_fail(r);
        unsigned char c = (unsigned char)*r->p++;
        char ch;
        if(c == '"') break;
        if(c < 0x20) return jr_fail(r);
        if(c == '\\') {
            if(r->p >= r->end) return jr_fail(r);
            char e = *r->p++;
            switch(e) {
            case '"':
            case '\\':
            case '/':
                ch = e;
                break;
            case 'b':
            case 'f':
            case 'n':
            case 'r':
            case 't':
                ch = '?';
                break;
            case 'u': {
                unsigned cp;
                if(!jr_hex4(r, &cp)) return false;
                /* A high surrogate followed by an escaped low surrogate is
                 * one code point. */
                if(cp >= 0xD800 && cp <= 0xDBFF && r->end - r->p >= 6 && r->p[0] == '\\' &&
                   r->p[1] == 'u') {
                    const char* save = r->p;
                    unsigned low;
                    r->p += 2;
                    if(!jr_hex4(r, &low)) return false;
                    if(low < 0xDC00 || low > 0xDFFF) r->p = save;
                }
                ch = (cp >= 0x20 && cp < 0x7F) ? (char)cp : '?';
                break;
            }
            default:
                return jr_fail(r);
            }
        } else if(c >= 0x80 && c < 0xC0) {
            continue; /* UTF-8 continuation byte: its lead byte gave the '?' */
        } else {
            ch = (c < 0x7F) ? (char)c : '?';
        }
        if(out) {
            if(n + 1 < cap)
                out[n++] = ch;
            else
                *overflow = true;
        }
    }
    if(out && cap > 0) out[n] = '\0';
    return true;
}

/* Scans a JSON number; `integral` tells whether it had no fraction and no
 * exponent. */
static bool jr_number(JsonReader* r, bool* integral) {
    if(!r->ok) return false;
    jr_skip_ws(r);
    const char* p = r->p;
    if(p < r->end && *p == '-') p++;
    if(p >= r->end || !jr_is_digit(*p)) return jr_fail(r);
    if(*p == '0')
        p++;
    else
        while(p < r->end && jr_is_digit(*p))
            p++;
    *integral = true;
    if(p < r->end && *p == '.') {
        *integral = false;
        p++;
        if(p >= r->end || !jr_is_digit(*p)) return jr_fail(r);
        while(p < r->end && jr_is_digit(*p))
            p++;
    }
    if(p < r->end && (*p == 'e' || *p == 'E')) {
        *integral = false;
        p++;
        if(p < r->end && (*p == '+' || *p == '-')) p++;
        if(p >= r->end || !jr_is_digit(*p)) return jr_fail(r);
        while(p < r->end && jr_is_digit(*p))
            p++;
    }
    /* A leading zero followed by more digits ("01") is not a number. */
    if(p < r->end && jr_is_digit(*p)) return jr_fail(r);
    r->p = p;
    return true;
}

bool json_object_begin(JsonReader* r) {
    return jr_open(r, '{');
}

bool json_object_next(JsonReader* r, char* key, size_t cap) {
    if(!jr_next(r, '}')) return false;
    bool overflow;
    if(cap == 0) key = NULL;
    if(!jr_string(r, key, cap, &overflow)) return false;
    if(overflow) key[0] = '\0';
    if(jr_peek_char(r) != ':') return jr_fail(r);
    r->p++;
    return true;
}

bool json_array_begin(JsonReader* r) {
    return jr_open(r, '[');
}

bool json_array_next(JsonReader* r) {
    return jr_next(r, ']');
}

bool json_read_string(JsonReader* r, char* out, size_t cap) {
    if(cap == 0) return jr_fail(r);
    bool overflow;
    if(!jr_string(r, out, cap, &overflow)) return false;
    if(overflow) return jr_fail(r);
    return true;
}

bool json_read_int(JsonReader* r, long* out) {
    if(!r->ok) return false;
    jr_skip_ws(r);
    const char* start = r->p;
    bool integral;
    if(!jr_number(r, &integral)) return false;
    if(!integral) return jr_fail(r);
    bool negative = *start == '-';
    long v = 0;
    for(const char* p = negative ? start + 1 : start; p < r->p; p++) {
        int d = *p - '0';
        if(negative) {
            if(v < (LONG_MIN + d) / 10) return jr_fail(r);
            v = v * 10 - d;
        } else {
            if(v > (LONG_MAX - d) / 10) return jr_fail(r);
            v = v * 10 + d;
        }
    }
    *out = v;
    return true;
}

bool json_read_bool(JsonReader* r, bool* out) {
    if(!r->ok) return false;
    char c = jr_peek_char(r);
    if(c == 't') {
        if(!jr_literal(r, "true")) return false;
        *out = true;
        return true;
    }
    if(c == 'f') {
        if(!jr_literal(r, "false")) return false;
        *out = false;
        return true;
    }
    return jr_fail(r);
}

bool json_skip(JsonReader* r) {
    bool flag;
    switch(json_peek(r)) {
    case JsonTypeObject:
        if(!json_object_begin(r)) return false;
        while(json_object_next(r, NULL, 0))
            if(!json_skip(r)) return false;
        return r->ok;
    case JsonTypeArray:
        if(!json_array_begin(r)) return false;
        while(json_array_next(r))
            if(!json_skip(r)) return false;
        return r->ok;
    case JsonTypeString:
        return jr_string(r, NULL, 0, &flag);
    case JsonTypeNumber:
        return jr_number(r, &flag);
    case JsonTypeBool:
        return json_read_bool(r, &flag);
    case JsonTypeNull:
        return jr_literal(r, "null");
    default:
        return jr_fail(r);
    }
}

bool json_reader_ok(const JsonReader* r) {
    return r->ok;
}

bool json_reader_finish(JsonReader* r) {
    if(!r->ok) return false;
    jr_skip_ws(r);
    return r->p == r->end;
}
