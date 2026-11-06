#include "ui_text.h"

#include <string.h>

#define ELLIPSIS     "..."
#define ELLIPSIS_LEN 3
#define FIT_MAX      64 /* longer text is cut to this first; no line is that wide */

void ui_text_fit(char* text, uint16_t max_width, UiMeasure measure, void* ctx) {
    if(measure(ctx, text) <= max_width) return;
    char orig[FIT_MAX];
    size_t len = strlen(text);
    if(len >= FIT_MAX) len = FIT_MAX - 1;
    memcpy(orig, text, len);
    for(size_t keep = len; keep-- > 0;) {
        if(keep + ELLIPSIS_LEN > len) continue; /* the dots must fit in the string */
        memcpy(text, orig, keep);
        memcpy(text + keep, ELLIPSIS, ELLIPSIS_LEN + 1);
        if(measure(ctx, text) <= max_width) return;
    }
    /* not even the dots fit: as many characters as do */
    for(size_t keep = len; keep-- > 0;) {
        memcpy(text, orig, keep);
        text[keep] = '\0';
        if(measure(ctx, text) <= max_width) return;
    }
}

bool ui_text_wrap(
    const char* text,
    size_t* pos,
    char* out,
    size_t cap,
    uint16_t max_width,
    UiMeasure measure,
    void* ctx) {
    if(cap == 0) return false;
    const char* p = text + *pos;
    out[0] = '\0';
    if(*p == '\0') return false;
    size_t len = 0;
    size_t brk = 0; /* line length at the last space, 0 when none */
    size_t brk_next = 0; /* where the next line starts after it */
    size_t i = 0;
    for(;; i++) {
        char c = p[i];
        if(c == '\0' || c == '\n') {
            *pos += i + (c == '\n');
            return true;
        }
        bool full = len + 1 >= cap;
        if(!full) {
            out[len] = c;
            out[len + 1] = '\0';
            full = measure(ctx, out) > max_width;
            out[len] = '\0';
        }
        if(full) break;
        if(c == ' ') {
            brk = len;
            brk_next = i + 1;
        }
        out[len++] = c;
        out[len] = '\0';
    }
    if(p[i] == ' ') {
        brk = len;
        brk_next = i;
    }
    if(brk > 0) {
        out[brk] = '\0';
        i = brk_next;
    } else if(len == 0) {
        /* not one character fits: take it anyway, or the text never ends */
        out[len++] = p[i++];
        out[len] = '\0';
    }
    while(p[i] == ' ')
        i++;
    *pos += i;
    return true;
}

static const char hex_digits[] = "0123456789ABCDEF";

static size_t
    put_hex(const uint8_t* bytes, size_t n, char* out, size_t cap, size_t at, bool colons) {
    for(size_t i = 0; i < n; i++) {
        if(colons && i > 0 && at + 1 < cap) out[at++] = ':';
        if(at + 2 >= cap) break;
        out[at++] = hex_digits[bytes[i] >> 4];
        out[at++] = hex_digits[bytes[i] & 0x0F];
    }
    out[at] = '\0';
    return at;
}

size_t ui_uid_short(const uint8_t* uid, size_t len, char* out, size_t cap) {
    if(cap == 0) return 0;
    if(len <= 4) return put_hex(uid, len, out, cap, 0, false);
    size_t at = put_hex(uid, 2, out, cap, 0, false);
    for(size_t i = 0; i < ELLIPSIS_LEN && at + 1 < cap; i++)
        out[at++] = '.';
    return put_hex(uid + len - 2, 2, out, cap, at, false);
}

size_t ui_uid_colons(const uint8_t* uid, size_t len, char* out, size_t cap) {
    if(cap == 0) return 0;
    return put_hex(uid, len, out, cap, 0, true);
}

uint8_t ui_luminance(uint32_t rgba) {
    unsigned r = (rgba >> 24) & 0xFF;
    unsigned g = (rgba >> 16) & 0xFF;
    unsigned b = (rgba >> 8) & 0xFF;
    return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

bool ui_bayer_ink(int x, int y, uint8_t luminance) {
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5},
    };
    unsigned threshold = bayer[y & 3][x & 3];
    /* ink when the threshold, centred in its 1/16 step, is below the darkness */
    return (2 * threshold + 1) * 255 < 32u * (255u - luminance);
}

const char* ui_tag_type_name(const TagDump* dump) {
    if(dump->type == TagTypeMfc) {
        return dump->sak == 0x18 ? "MIFARE Classic 4K" : "MIFARE Classic 1K";
    }
    switch(dump->page_count) {
    case 45:
        return "NTAG213";
    case 135:
        return "NTAG215";
    case 231:
        return "NTAG216";
    default:
        return "NTAG";
    }
}

static size_t put_str(char* out, size_t cap, size_t at, const char* s) {
    while(*s && at + 1 < cap)
        out[at++] = *s++;
    out[at] = '\0';
    return at;
}

static char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* Whether `s` starts with `prefix` (non-empty), ignoring ASCII case. */
static bool starts_with_nocase(const char* s, const char* prefix) {
    for(; *prefix; s++, prefix++)
        if(lower(*s) != lower(*prefix)) return false;
    return true;
}

size_t ui_spool_name(const char* material, const char* subtype, char* out, size_t cap) {
    if(cap == 0) return 0;
    out[0] = '\0';
    size_t mlen = strlen(material);
    bool subtype_has_material = mlen > 0 && starts_with_nocase(subtype, material) &&
                                (subtype[mlen] == '\0' || subtype[mlen] == ' ');
    size_t at = 0;
    if(!subtype_has_material) {
        at = put_str(out, cap, at, material);
        if(at > 0 && subtype[0]) at = put_str(out, cap, at, " ");
    }
    return put_str(out, cap, at, subtype);
}
