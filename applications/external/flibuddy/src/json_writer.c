#include "json_writer.h"

static const char json_hex_digits[] = "0123456789ABCDEF";

void json_init(JsonWriter* w, char* buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->ok = cap > 0;
    w->need_comma = false;
    if(cap > 0) buf[0] = '\0';
}

static bool json_fail(JsonWriter* w) {
    w->ok = false;
    return false;
}

static bool json_put(JsonWriter* w, char c) {
    if(!w->ok) return false;
    if(w->len + 1 >= w->cap) {
        w->buf[w->len] = '\0';
        return json_fail(w);
    }
    w->buf[w->len++] = c;
    w->buf[w->len] = '\0';
    return true;
}

static bool json_put_str(JsonWriter* w, const char* s) {
    while(*s)
        if(!json_put(w, *s++)) return false;
    return w->ok;
}

static bool json_comma(JsonWriter* w) {
    if(w->need_comma && !json_put(w, ',')) return false;
    w->need_comma = false;
    return w->ok;
}

static bool json_open(JsonWriter* w, char c) {
    if(!json_comma(w) || !json_put(w, c)) return false;
    w->need_comma = false;
    return true;
}

static bool json_close(JsonWriter* w, char c) {
    if(!json_put(w, c)) return false;
    w->need_comma = true;
    return true;
}

bool json_object_open(JsonWriter* w) {
    return json_open(w, '{');
}

bool json_object_close(JsonWriter* w) {
    return json_close(w, '}');
}

bool json_array_open(JsonWriter* w) {
    return json_open(w, '[');
}

bool json_array_close(JsonWriter* w) {
    return json_close(w, ']');
}

static bool json_escaped(JsonWriter* w, const char* s) {
    if(!json_put(w, '"')) return false;
    for(const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        bool ok;
        switch(c) {
        case '"':
            ok = json_put_str(w, "\\\"");
            break;
        case '\\':
            ok = json_put_str(w, "\\\\");
            break;
        case '\n':
            ok = json_put_str(w, "\\n");
            break;
        case '\r':
            ok = json_put_str(w, "\\r");
            break;
        case '\t':
            ok = json_put_str(w, "\\t");
            break;
        default:
            if(c < 0x20) {
                ok = json_put_str(w, "\\u00") && json_put(w, json_hex_digits[c >> 4]) &&
                     json_put(w, json_hex_digits[c & 0x0F]);
            } else {
                ok = json_put(w, c >= 0x80 ? '?' : (char)c);
            }
        }
        if(!ok) return false;
    }
    return json_put(w, '"');
}

bool json_key(JsonWriter* w, const char* key) {
    if(!json_comma(w) || !json_escaped(w, key) || !json_put(w, ':')) return false;
    w->need_comma = false;
    return true;
}

static bool json_value_done(JsonWriter* w) {
    if(!w->ok) return false;
    w->need_comma = true;
    return true;
}

bool json_string(JsonWriter* w, const char* s) {
    if(!json_comma(w) || !json_escaped(w, s)) return false;
    return json_value_done(w);
}

bool json_int(JsonWriter* w, long value) {
    if(!json_comma(w)) return false;
    /* Work on the magnitude as unsigned so LONG_MIN does not overflow. */
    unsigned long magnitude = value < 0 ? 0UL - (unsigned long)value : (unsigned long)value;
    char digits[24];
    size_t n = 0;
    do {
        digits[n++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while(magnitude > 0);
    if(value < 0 && !json_put(w, '-')) return false;
    while(n > 0)
        if(!json_put(w, digits[--n])) return false;
    return json_value_done(w);
}

bool json_bool(JsonWriter* w, bool value) {
    if(!json_comma(w) || !json_put_str(w, value ? "true" : "false")) return false;
    return json_value_done(w);
}

bool json_hex(JsonWriter* w, const uint8_t* bytes, size_t len) {
    if(!json_comma(w) || !json_put(w, '"')) return false;
    for(size_t i = 0; i < len; i++)
        if(!json_put(w, json_hex_digits[bytes[i] >> 4]) ||
           !json_put(w, json_hex_digits[bytes[i] & 0x0F]))
            return false;
    if(!json_put(w, '"')) return false;
    return json_value_done(w);
}

bool json_ok(const JsonWriter* w) {
    return w->ok;
}

size_t json_len(const JsonWriter* w) {
    return w->ok ? w->len : 0;
}
