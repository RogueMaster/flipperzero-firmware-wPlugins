#include "ctf_codec.h"
#include <string.h>

/* A printable byte passes through; anything else becomes a dot so the LCD is
 * never asked to render control characters. */
static char printable(int c) {
    return (c >= 0x20 && c < 0x7f) ? (char)c : '.';
}

static void out_put(char* out, size_t out_sz, size_t* n, char c) {
    if(*n < out_sz - 1) out[(*n)++] = c;
}

/* ------------------------------------------------------------------- ROT-N */
static void do_rot(const char* in, int shift, char* out, size_t out_sz) {
    shift = ((shift % 26) + 26) % 26;
    size_t n = 0;
    for(const char* p = in; *p && n < out_sz - 1; p++) {
        char c = *p;
        if(c >= 'a' && c <= 'z')
            c = (char)('a' + (c - 'a' + shift) % 26);
        else if(c >= 'A' && c <= 'Z')
            c = (char)('A' + (c - 'A' + shift) % 26);
        out[n++] = c;
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------ Atbash */
static void do_atbash(const char* in, char* out, size_t out_sz) {
    size_t n = 0;
    for(const char* p = in; *p && n < out_sz - 1; p++) {
        char c = *p;
        if(c >= 'a' && c <= 'z')
            c = (char)('z' - (c - 'a'));
        else if(c >= 'A' && c <= 'Z')
            c = (char)('Z' - (c - 'A'));
        out[n++] = c;
    }
    out[n] = '\0';
}

/* ----------------------------------------------------------------- reverse */
static void do_reverse(const char* in, char* out, size_t out_sz) {
    size_t len = strlen(in);
    if(len > out_sz - 1) len = out_sz - 1;
    for(size_t i = 0; i < len; i++) out[i] = in[len - 1 - i];
    out[len] = '\0';
}

/* --------------------------------------------------------------------- hex */
static int hex_val(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void do_hex(const char* in, char* out, size_t out_sz) {
    size_t n = 0;
    int hi = -1;
    for(const char* p = in; *p && n < out_sz - 1; p++) {
        int v = hex_val(*p);
        if(v < 0) continue; /* skip spaces, colons, 0x, etc. */
        if(hi < 0) {
            hi = v;
        } else {
            out_put(out, out_sz, &n, printable((hi << 4) | v));
            hi = -1;
        }
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------ binary */
static void do_binary(const char* in, char* out, size_t out_sz) {
    size_t n = 0;
    int bits = 0;
    unsigned acc = 0;
    for(const char* p = in; *p && n < out_sz - 1; p++) {
        if(*p != '0' && *p != '1') continue; /* group separators ignored */
        acc = (acc << 1) | (unsigned)(*p - '0');
        if(++bits == 8) {
            out_put(out, out_sz, &n, printable((int)acc));
            bits = 0;
            acc = 0;
        }
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------- base64 */
static int b64_val(char c) {
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}

static void do_base64(const char* in, char* out, size_t out_sz) {
    size_t n = 0;
    int q = 0;
    unsigned acc = 0;
    for(const char* p = in; *p && n < out_sz - 1; p++) {
        int v = b64_val(*p);
        if(v < 0) continue; /* skip '=', newlines, spaces */
        acc = (acc << 6) | (unsigned)v;
        if(++q == 4) {
            out_put(out, out_sz, &n, printable((int)((acc >> 16) & 0xff)));
            out_put(out, out_sz, &n, printable((int)((acc >> 8) & 0xff)));
            out_put(out, out_sz, &n, printable((int)(acc & 0xff)));
            q = 0;
            acc = 0;
        }
    }
    /* trailing group (2 or 3 chars = 1 or 2 bytes) */
    if(q == 2) {
        out_put(out, out_sz, &n, printable((int)((acc >> 4) & 0xff)));
    } else if(q == 3) {
        out_put(out, out_sz, &n, printable((int)((acc >> 10) & 0xff)));
        out_put(out, out_sz, &n, printable((int)((acc >> 2) & 0xff)));
    }
    out[n] = '\0';
}

/* -------------------------------------------------------------------- Morse */
/* index by A..Z then 0..9 */
static const char* const MORSE[] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....", "..", /* A-I */
    ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.", "--.-", ".-.", /* J-R */
    "...",  "-",    "..-",  "...-", ".--",  "-..-", "-.--", "--..", /* S-Z */
    "-----", ".----", "..---", "...--", "....-", /* 0-4 */
    ".....", "-....", "--...", "---..", "----.", /* 5-9 */
};

static char morse_lookup(const char* tok) {
    if(tok[0] == '\0') return '\0';
    for(int i = 0; i < 26; i++)
        if(strcmp(tok, MORSE[i]) == 0) return (char)('A' + i);
    for(int i = 0; i < 10; i++)
        if(strcmp(tok, MORSE[26 + i]) == 0) return (char)('0' + i);
    return '?';
}

static void do_morse(const char* in, char* out, size_t out_sz) {
    size_t n = 0;
    char tok[8];
    size_t tl = 0;
    int spaces = 0; /* consecutive separators, to detect word gaps ('/' or run) */
    for(const char* p = in;; p++) {
        char c = *p;
        if(c == '.' || c == '-') {
            if(tl < sizeof(tok) - 1) tok[tl++] = c;
            spaces = 0;
        } else {
            /* boundary: flush the current letter */
            if(tl > 0) {
                tok[tl] = '\0';
                char d = morse_lookup(tok);
                if(d) out_put(out, out_sz, &n, d);
                tl = 0;
            }
            if(c == '/') {
                out_put(out, out_sz, &n, ' ');
                spaces = 0;
            } else if(c == ' ') {
                if(++spaces == 3) { /* long run of spaces = word gap */
                    out_put(out, out_sz, &n, ' ');
                    spaces = 0;
                }
            }
            if(c == '\0') break;
        }
    }
    out[n] = '\0';
}

/* --------------------------------------------------------------------- api */
bool codec_uses_param(CodecId id) {
    return id == CodecRot;
}

const char* codec_name(CodecId id, int param) {
    static char rotbuf[10];
    switch(id) {
    case CodecRot: {
        int s = ((param % 26) + 26) % 26;
        rotbuf[0] = 'R';
        rotbuf[1] = 'O';
        rotbuf[2] = 'T';
        rotbuf[3] = '-';
        if(s >= 10) {
            rotbuf[4] = (char)('0' + s / 10);
            rotbuf[5] = (char)('0' + s % 10);
            rotbuf[6] = '\0';
        } else {
            rotbuf[4] = (char)('0' + s);
            rotbuf[5] = '\0';
        }
        return rotbuf;
    }
    case CodecBase64: return "Base64";
    case CodecHex: return "Hex -> Text";
    case CodecBinary: return "Binary -> Text";
    case CodecMorse: return "Morse -> Text";
    case CodecAtbash: return "Atbash";
    case CodecReverse: return "Reverse";
    default: return "?";
    }
}

void codec_apply(CodecId id, const char* in, int param, char* out, size_t out_sz) {
    if(out_sz == 0) return;
    switch(id) {
    case CodecRot: do_rot(in, param, out, out_sz); break;
    case CodecBase64: do_base64(in, out, out_sz); break;
    case CodecHex: do_hex(in, out, out_sz); break;
    case CodecBinary: do_binary(in, out, out_sz); break;
    case CodecMorse: do_morse(in, out, out_sz); break;
    case CodecAtbash: do_atbash(in, out, out_sz); break;
    case CodecReverse: do_reverse(in, out, out_sz); break;
    default: out[0] = '\0'; break;
    }
}
