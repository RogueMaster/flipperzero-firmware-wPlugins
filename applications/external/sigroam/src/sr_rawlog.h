#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ Ring buffer for malformed lines. Must not include any device runtime header (ADR-003).
 * Per ADR-018. It accepts only SrEventUnknown (hooked in apply_unknown).
 *
 * Input follows the ADR-010 readable-range contract: read only [text, text+len), and never use
 * C string functions that scan for a NUL.
 */

enum { SR_RAWLOG_LINES = 16, SR_RAWLOG_LINE_MAX = 80 };

typedef struct {
    char text[SR_RAWLOG_LINE_MAX + 1];
    uint8_t len;
    bool cut;
} SrRawLogEntry;

typedef struct {
    SrRawLogEntry e[SR_RAWLOG_LINES];
    size_t head;
    size_t count;
    uint32_t pushed;
} SrRawLog;

static inline void sr_rawlog_init(SrRawLog* l) {
    size_t i;
    size_t j;

    if(l == NULL) {
        return;
    }
    for(i = 0; i < (size_t)SR_RAWLOG_LINES; i++) {
        for(j = 0; j <= (size_t)SR_RAWLOG_LINE_MAX; j++) {
            l->e[i].text[j] = '\0';
        }
        l->e[i].len = 0;
        l->e[i].cut = false;
    }
    l->head = 0;
    l->count = 0;
    l->pushed = 0;
}

static inline char sr_rawlog_sanitize(unsigned char c) {
    /* V-058 (5) + V-044 (5): every non-printable ASCII byte becomes '.', including \r/\n/\t. */
    if(c < 0x20u || c > 0x7Eu) {
        return '.';
    }
    return (char)c;
}

static inline void sr_rawlog_push(SrRawLog* l, const char* text, size_t len) {
    SrRawLogEntry* slot;
    size_t n;
    size_t i;

    if(l == NULL || text == NULL) {
        return;
    }

    slot = &l->e[l->head];
    n = len;
    slot->cut = false;
    if(n > (size_t)SR_RAWLOG_LINE_MAX) {
        n = (size_t)SR_RAWLOG_LINE_MAX;
        slot->cut = true;
    }
    for(i = 0; i < n; i++) {
        slot->text[i] = sr_rawlog_sanitize((unsigned char)text[i]);
    }
    slot->text[n] = '\0';
    slot->len = (uint8_t)n;

    l->head++;
    if(l->head == (size_t)SR_RAWLOG_LINES) {
        l->head = 0;
    }
    if(l->count < (size_t)SR_RAWLOG_LINES) {
        l->count++;
    }
    l->pushed++;
}

static inline int sr_rawlog_putc(char* out, size_t cap, size_t* n, char c) {
    if(*n + 1u >= cap) {
        return 0;
    }
    out[*n] = c;
    (*n)++;
    return 1;
}

static inline size_t sr_rawlog_render(const SrRawLog* l, char* out, size_t cap) {
    size_t n = 0;
    size_t i;
    size_t oldest;
    int ok;

    if(out == NULL || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if(l == NULL || l->count == 0) {
        return 0;
    }

    oldest = (l->head + (size_t)SR_RAWLOG_LINES - l->count) % (size_t)SR_RAWLOG_LINES;
    ok = 1;
    for(i = 0; ok && i < l->count; i++) {
        const SrRawLogEntry* ent = &l->e[(oldest + i) % (size_t)SR_RAWLOG_LINES];
        size_t k;

        for(k = 0; ok && k < (size_t)ent->len; k++) {
            ok = sr_rawlog_putc(out, cap, &n, ent->text[k]);
        }
        if(ok && ent->cut) {
            ok = sr_rawlog_putc(out, cap, &n, '>');
        }
        if(ok) {
            ok = sr_rawlog_putc(out, cap, &n, '\n');
        }
    }
    out[n] = '\0';
    return n;
}

/* Whether the rawlog needs re-rendering into the view.
 * pushed is a uint32_t that only ever grows -- the comparison must use != and must not use >
 * (which would stop refreshing forever at the wraparound point). */
static inline bool sr_rawlog_should_render(uint32_t pushed_now, uint32_t pushed_shown) {
    return pushed_now != pushed_shown;
}
