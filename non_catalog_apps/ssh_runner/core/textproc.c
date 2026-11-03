/* SPDX-License-Identifier: GPL-3.0-only */
#include "textproc.h"

#include <proto/proto_msg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum {
    AnsiNormal = 0,
    AnsiEsc, /* after ESC */
    AnsiCsi, /* ESC [ ... */
    AnsiOsc, /* ESC ] ... */
    AnsiOscEsc, /* ESC inside OSC, expecting '\' */
    AnsiEscInter, /* ESC + intermediate (e.g. ESC ( B) */
};

void ansi_init(AnsiStripper* a) {
    memset(a, 0, sizeof(*a));
}

size_t ansi_strip(AnsiStripper* a, const uint8_t* in, size_t len, char* out) {
    size_t o = 0;
    for(size_t i = 0; i < len; i++) {
        uint8_t c = in[i];

        if(a->pending_cr) {
            a->pending_cr = false;
            out[o++] = '\n';
            if(c == '\n') continue; /* CR LF -> one LF */
        }

        switch(a->state) {
        case AnsiEsc:
            if(c == '[')
                a->state = AnsiCsi;
            else if(c == ']')
                a->state = AnsiOsc;
            else if(c >= 0x20 && c <= 0x2F)
                a->state = AnsiEscInter;
            else
                a->state = AnsiNormal; /* two-byte sequence, consumed */
            continue;
        case AnsiEscInter:
            if(c < 0x20 || c > 0x2F) a->state = AnsiNormal;
            continue;
        case AnsiCsi:
            if(c >= 0x40 && c <= 0x7E)
                a->state = AnsiNormal; /* final byte */
            else if(c == 0x1B)
                a->state = AnsiEsc;
            continue;
        case AnsiOsc:
            if(c == 0x07)
                a->state = AnsiNormal;
            else if(c == 0x1B)
                a->state = AnsiOscEsc;
            continue;
        case AnsiOscEsc:
            a->state = (c == '\\') ? AnsiNormal : AnsiOsc;
            continue;
        default:
            break;
        }

        if(a->utf8_left) {
            if((c & 0xC0) == 0x80) {
                a->utf8_left--;
                continue;
            }
            a->utf8_left = 0; /* malformed; fall through and treat c normally */
        }

        if(c == 0x1B) {
            a->state = AnsiEsc;
        } else if(c == '\r') {
            a->pending_cr = true;
        } else if(c == '\n') {
            out[o++] = '\n';
        } else if(c == '\t') {
            out[o++] = ' ';
            out[o++] = ' ';
        } else if(c < 0x20 || c == 0x7F) {
            /* other C0 controls (BEL, BS, ...) dropped */
        } else if(c < 0x80) {
            out[o++] = (char)c;
        } else if((c & 0xE0) == 0xC0) {
            a->utf8_left = 1;
            out[o++] = '?';
        } else if((c & 0xF0) == 0xE0) {
            a->utf8_left = 2;
            out[o++] = '?';
        } else if((c & 0xF8) == 0xF0) {
            a->utf8_left = 3;
            out[o++] = '?';
        } else {
            out[o++] = '?'; /* stray continuation / invalid lead byte */
        }
    }
    return o;
}

/* ---------------- output buffer ---------------- */

void outbuf_reset(OutBuf* o) {
    o->head_len = 0;
    o->tail_len = 0;
    o->total = 0;
}

void outbuf_append(OutBuf* o, const char* data, size_t len) {
    o->total += len;
    size_t h = OUTBUF_HEAD - o->head_len;
    if(h > len) h = len;
    memcpy(&o->buf[o->head_len], data, h);
    o->head_len += h;
    data += h;
    len -= h;
    if(!len) return;
    char* tail = o->buf + OUTBUF_HEAD;
    if(len >= OUTBUF_TAIL) {
        memcpy(tail, data + len - OUTBUF_TAIL, OUTBUF_TAIL);
        o->tail_len = OUTBUF_TAIL;
        return;
    }
    /* drop the oldest bytes by moving the rest down: the tail stays one run (shown in
     * place); at most 3 KB per append, a few % of the CPU at full UART speed */
    size_t room = OUTBUF_TAIL - o->tail_len;
    if(len > room) {
        size_t drop = len - room;
        memmove(tail, tail + drop, o->tail_len - drop);
        o->tail_len -= drop;
    }
    memcpy(tail + o->tail_len, data, len);
    o->tail_len += len;
}

size_t outbuf_dropped(const OutBuf* o) {
    return o->total - o->head_len - o->tail_len;
}

bool outbuf_truncated(const OutBuf* o) {
    return outbuf_dropped(o) > 0;
}

size_t outbuf_render_size(const OutBuf* o) {
    return o->head_len + o->tail_len + 48;
}

void outbuf_parts(const OutBuf* o, OutbufParts* p) {
    p->head = o->buf;
    p->tail = o->buf + OUTBUF_HEAD;
    p->marker[0] = '\0';
    p->marker_len = 0;
    if(!outbuf_truncated(o)) {
        /* head and tail are adjacent: one run */
        p->head_len = o->head_len + o->tail_len;
        p->tail_len = 0;
        return;
    }
    p->head_len = o->head_len;
    /* Start the tail at a line boundary so no half line is shown. */
    size_t skip = 0;
    const char* nl = memchr(p->tail, '\n', o->tail_len);
    if(nl && (size_t)(nl - p->tail) + 1 < o->tail_len) skip = (size_t)(nl - p->tail) + 1;
    p->tail += skip;
    p->tail_len = o->tail_len - skip;
    char sz[16];
    format_size(sz, sizeof(sz), (uint32_t)(outbuf_dropped(o) + skip));
    bool nlh = o->head_len && o->buf[o->head_len - 1] != '\n';
    int m =
        snprintf(p->marker, sizeof(p->marker), "%s[... %s skipped ...]\n", nlh ? "\n" : "", sz);
    if(m > 0) p->marker_len = (size_t)m < sizeof(p->marker) ? (size_t)m : sizeof(p->marker) - 1;
}

void outbuf_emit(const OutBuf* o, OutbufSink sink, void* ctx) {
    OutbufParts p;
    outbuf_parts(o, &p);
    sink(ctx, p.head, p.head_len);
    if(p.marker_len) sink(ctx, p.marker, p.marker_len);
    if(p.tail_len) sink(ctx, p.tail, p.tail_len);
}

typedef struct {
    char* dst;
    size_t cap;
    size_t n;
} BufSink;

static void buf_sink(void* ctx, const char* data, size_t len) {
    BufSink* b = ctx;
    size_t room = b->cap - 1 - b->n;
    size_t k = len < room ? len : room;
    memcpy(b->dst + b->n, data, k);
    b->n += k;
}

size_t outbuf_render(const OutBuf* o, char* dst, size_t cap) {
    if(!cap) return 0;
    BufSink b = {dst, cap, 0};
    outbuf_emit(o, buf_sink, &b);
    dst[b.n] = '\0';
    return b.n;
}

/* ---------------- display ---------------- */

char* text_ascii(const char* src, char* dst, size_t size, char missing) {
    if(!size) return dst;
    size_t n = 0;
    const unsigned char* p = (const unsigned char*)(src ? src : "");
    while(*p && n + 1 < size) {
        unsigned char c = *p++;
        if(c >= 0x20 && c < 0x7F) {
            dst[n++] = (char)c;
            continue;
        }
        dst[n++] = missing;
        /* the rest of a UTF-8 sequence is part of the same character */
        if(c >= 0xC0)
            while((*p & 0xC0) == 0x80)
                p++;
    }
    dst[n] = '\0';
    return dst;
}

/* ---------------- formatting ---------------- */

void format_duration(char* buf, size_t size, uint32_t ms) {
    if(ms < 1000)
        snprintf(buf, size, "%lu ms", (unsigned long)ms);
    else if(ms < 60000)
        snprintf(
            buf, size, "%lu.%lu s", (unsigned long)(ms / 1000), (unsigned long)((ms % 1000) / 100));
    else
        snprintf(
            buf,
            size,
            "%lu m %02lu s",
            (unsigned long)(ms / 60000),
            (unsigned long)((ms / 1000) % 60));
}

void format_size(char* buf, size_t size, uint32_t bytes) {
    if(bytes < 1024)
        snprintf(buf, size, "%lu B", (unsigned long)bytes);
    else
        snprintf(
            buf,
            size,
            "%lu.%lu KB",
            (unsigned long)(bytes / 1024),
            (unsigned long)((bytes % 1024) * 10 / 1024));
}

void format_exit_line(
    char* buf,
    size_t size,
    int32_t exit_code,
    uint8_t done_flags,
    uint32_t duration_ms) {
    char d[16];
    format_duration(d, sizeof(d), duration_ms);
    if(done_flags & PROTO_DONE_LOST)
        snprintf(buf, size, "connection lost, %s", d);
    else if(done_flags & PROTO_DONE_CANCELLED)
        snprintf(buf, size, "cancelled, %s", d);
    else if(done_flags & PROTO_DONE_SIGNAL)
        snprintf(buf, size, "signal %ld, %s", (long)exit_code, d);
    else
        snprintf(buf, size, "exit %ld, %s", (long)exit_code, d);
}

void ip4_format(uint32_t ip, char* buf, size_t size) {
    snprintf(
        buf,
        size,
        "%lu.%lu.%lu.%lu",
        (unsigned long)((ip >> 24) & 0xFF),
        (unsigned long)((ip >> 16) & 0xFF),
        (unsigned long)((ip >> 8) & 0xFF),
        (unsigned long)(ip & 0xFF));
}

bool ip4_parse(const char* s, uint32_t* out) {
    uint32_t ip = 0;
    for(int part = 0; part < 4; part++) {
        if(*s < '0' || *s > '9') return false;
        uint32_t v = 0;
        int digits = 0;
        while(*s >= '0' && *s <= '9') {
            v = v * 10 + (uint32_t)(*s - '0');
            s++;
            if(++digits > 3 || v > 255) return false;
        }
        ip = (ip << 8) | v;
        if(part < 3) {
            if(*s != '.') return false;
            s++;
        }
    }
    if(*s != '\0') return false;
    *out = ip;
    return true;
}

void fp_wrap(const char* fp, size_t width, char* out, size_t size) {
    size_t o = 0, col = 0;
    if(!size) return;
    for(const char* p = fp; *p && o + 2 < size; p++) {
        if(col == width) {
            out[o++] = '\n';
            col = 0;
        }
        out[o++] = *p;
        col++;
    }
    out[o] = '\0';
}

const char* key_type_short(const char* t) {
    if(!t || !t[0]) return "Unknown";
    if(strstr(t, "ed25519")) return "ED25519";
    if(strstr(t, "nistp256")) return "ECDSA P-256";
    if(strstr(t, "nistp384")) return "ECDSA P-384";
    if(strstr(t, "nistp521")) return "ECDSA P-521";
    if(strstr(t, "ecdsa")) return "ECDSA";
    if(strstr(t, "rsa")) return "RSA";
    if(strstr(t, "dss")) return "DSA";
    return t;
}

static bool technical(const char* s, uint8_t dots_needed) {
    if(!s) return false;
    uint8_t dots = 0; /* '.' between digits in the current run of digits and dots */
    for(const char* p = s; *p; p++) {
        if(strchr("@/$#[]%~|\\", *p)) return true;
        if(*p == ':' && p[1] && p[1] != ' ' && p[1] != '\n') return true;
        if(*p == '.' && p > s && isdigit((unsigned char)p[-1]) && isdigit((unsigned char)p[1])) {
            if(++dots >= dots_needed) return true;
        } else if(!isdigit((unsigned char)*p)) {
            dots = 0;
        }
    }
    return false;
}

bool text_is_technical(const char* s) {
    return technical(s, 1);
}

bool text_is_technical_body(const char* s) {
    return technical(s, 2);
}
