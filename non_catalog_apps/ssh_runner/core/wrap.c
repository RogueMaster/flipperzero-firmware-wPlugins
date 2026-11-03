/* Word wrap and truncation (see wrap.h). SPDX-License-Identifier: GPL-3.0-only */
#include "wrap.h"

#include <string.h>

#define WRAP_MIN_FRAGMENT 3 /* a split word leaves at least this many characters below */

static bool break_after(char c) {
    return c == '-' || c == '/' || c == ',' || c == ':' || c == '=';
}

static bool is_space(char c) {
    return c == ' ' || c == '\t';
}

/* A hyphen is a break opportunity only between two parts of at least WRAP_MIN_FRAGMENT
 * characters: "add-on", "SD-card" and "x86-64" stay whole. Other break characters always. */
static bool hyphen_ok(const char* text, size_t len, size_t start, size_t j) {
    if(text[j] != '-') return true;
    size_t b = j;
    while(b > start && !is_space(text[b - 1]) && text[b - 1] != '-')
        b--;
    size_t a = j + 1;
    while(a < len && !is_space(text[a]) && text[a] != '\n' && text[a] != '-')
        a++;
    while(a > j + 1 &&
          (text[a - 1] == ',' || text[a - 1] == '.' || text[a - 1] == ';' || text[a - 1] == ':'))
        a--; /* "add-on," : the comma is not part of "on" */
    return j - b >= WRAP_MIN_FRAGMENT && a - (j + 1) >= WRAP_MIN_FRAGMENT;
}

uint32_t wrap_width(const char* s, size_t len, WrapGlyphWidth gw, void* ctx) {
    uint32_t w = 0;
    for(size_t i = 0; i < len; i++)
        if(s[i] != '\r') w += gw(ctx, s[i]);
    return w;
}

static void emit(WrapLine* out, size_t max_lines, size_t n, uint32_t off, size_t len) {
    if(out && n < max_lines) {
        out[n].off = off;
        out[n].len = (uint16_t)(len > 0xFFFF ? 0xFFFF : len);
    }
}

static size_t wrap_core(
    const char* text,
    size_t len,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    WrapLine* out,
    size_t max_lines,
    size_t stop, /* 0: lay out everything */
    size_t* next) {
    size_t n = 0;
    size_t i = 0;
    bool continuation = false; /* the previous line was wrapped, not ended by '\n' */
    if(next) *next = len;
    if(!len) return 0;
    while(i < len) {
        if(continuation)
            while(i < len && is_space(text[i]))
                i++;
        if(i >= len) break;
        if(stop && n >= stop) {
            if(next) *next = i;
            return n;
        }
        size_t start = i;
        uint32_t w = 0;
        size_t brk_end = 0, brk_next = 0; /* last break opportunity: line end, next start */
        size_t pbrk_end = 0, pbrk_next = 0; /* the one before it (widow control) */
        size_t j = i;
        for(; j < len && text[j] != '\n'; j++) {
            char c = text[j];
            if(c == '\r') continue;
            uint8_t cw = gw(ctx, c);
            if(w + cw > max_px) break;
            w += cw;
            if(is_space(c)) {
                if(j > start && brk_end != j) {
                    pbrk_end = brk_end;
                    pbrk_next = brk_next;
                    brk_end = j;
                    brk_next = j + 1;
                } else if(j > start) {
                    brk_next = j + 1;
                }
            } else if(break_after(c) && hyphen_ok(text, len, start, j)) {
                pbrk_end = brk_end;
                pbrk_next = brk_next;
                brk_end = j + 1;
                brk_next = j + 1;
            }
        }
        if(j >= len || text[j] == '\n') {
            size_t e = j;
            while(e > start && (is_space(text[e - 1]) || text[e - 1] == '\r'))
                e--;
            emit(out, max_lines, n++, (uint32_t)start, e - start);
            i = j + 1; /* past '\n' (or the end); a final '\n' adds no empty line */
            continuation = false;
            continue;
        }
        /* the line is full at j */
        if(brk_end > start) {
            /* widow control: a paragraph must not end with a lone 1-2 char word on a line
             * of its own; break one opportunity earlier so a word goes down with it */
            size_t k = brk_next;
            while(k < len && is_space(text[k]))
                k++;
            size_t rest = 0;
            while(k + rest < len && text[k + rest] != '\n' && text[k + rest] != '\r')
                rest++;
            if(rest > 0 && rest < WRAP_MIN_FRAGMENT && pbrk_end > start) {
                brk_end = pbrk_end;
                brk_next = pbrk_next;
            }
            size_t e = brk_end;
            while(e > start && is_space(text[e - 1]))
                e--;
            emit(out, max_lines, n++, (uint32_t)start, e - start);
            i = brk_next;
        } else {
            /* one word wider than the line: split it, but never leave a 1-2 char tail */
            size_t cut = j > start ? j : start + 1;
            size_t word_end = cut;
            while(word_end < len && !is_space(text[word_end]) && text[word_end] != '\n')
                word_end++;
            size_t tail = word_end - cut;
            if(tail > 0 && tail < WRAP_MIN_FRAGMENT) {
                size_t back = WRAP_MIN_FRAGMENT - tail;
                if(cut - start > back + 1) cut -= back;
            }
            emit(out, max_lines, n++, (uint32_t)start, cut - start);
            i = cut;
        }
        continuation = true;
    }
    return n;
}

size_t wrap_text(
    const char* text,
    size_t len,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    WrapLine* out,
    size_t max_lines) {
    return wrap_core(text, len, max_px, gw, ctx, out, max_lines, 0, NULL);
}

size_t wrap_next(
    const char* text,
    size_t len,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    WrapLine* out,
    size_t max_lines,
    size_t* next) {
    return wrap_core(text, len, max_px, gw, ctx, out, max_lines, max_lines ? max_lines : 1, next);
}

bool wrap_fit(
    const char* s,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    char* out,
    size_t out_size) {
    if(!out_size) return false;
    size_t len = strlen(s);
    if(wrap_width(s, len, gw, ctx) <= max_px) {
        size_t k = len < out_size - 1 ? len : out_size - 1;
        memcpy(out, s, k);
        out[k] = '\0';
        return k < len;
    }
    uint32_t dots = 3u * gw(ctx, '.');
    uint32_t w = 0;
    size_t k = 0;
    while(k < len && k + 4 < out_size) {
        uint8_t cw = gw(ctx, s[k]);
        if(w + cw + dots > max_px) break;
        w += cw;
        k++;
    }
    while(k > 0 && is_space(s[k - 1]))
        k--;
    memcpy(out, s, k);
    memcpy(out + k, "...", 4);
    return true;
}

bool wrap_fit_middle(
    const char* s,
    uint16_t max_px,
    WrapGlyphWidth gw,
    void* ctx,
    char* out,
    size_t out_size) {
    if(!out_size) return false;
    size_t len = strlen(s);
    if(wrap_width(s, len, gw, ctx) <= max_px && len < out_size) {
        memcpy(out, s, len + 1);
        return false;
    }
    /* the tail always keeps the extension (from the last '.', if it is short) */
    const char* dot = strrchr(s, '.');
    size_t tail = (dot && (size_t)(s + len - dot) <= 8) ? (size_t)(s + len - dot) : 0;
    size_t head = 0;
    uint32_t dots = 3u * gw(ctx, '.');
    /* grow head and tail in turn while "head...tail" fits */
    for(bool grew = true; grew;) {
        grew = false;
        if(head + tail < len && head + tail + 4 < out_size &&
           wrap_width(s, head + 1, gw, ctx) + dots + wrap_width(s + len - tail, tail, gw, ctx) <=
               max_px) {
            head++;
            grew = true;
        }
        if(head + tail < len && head + tail + 4 < out_size &&
           wrap_width(s, head, gw, ctx) + dots +
                   wrap_width(s + len - tail - 1, tail + 1, gw, ctx) <=
               max_px) {
            tail++;
            grew = true;
        }
    }
    memcpy(out, s, head);
    memcpy(out + head, "...", 3);
    memcpy(out + head + 3, s + len - tail, tail);
    out[head + 3 + tail] = '\0';
    return true;
}
