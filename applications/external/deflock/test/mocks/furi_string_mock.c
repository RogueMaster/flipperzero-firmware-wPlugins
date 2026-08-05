// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Host implementation of the minimal FuriString mock (see mocks/core/string.h):
// a plain growable char buffer, enough to exercise the modules that append
// human-readable reason strings.
#include "core/string.h"

#include <stdlib.h>
#include <string.h>

struct FuriString {
    char* buf;
    size_t len; /**< strlen(buf) */
    size_t cap; /**< allocated capacity incl. the NUL */
};

FuriString* furi_string_alloc(void) {
    FuriString* s = malloc(sizeof(*s));
    s->cap = 32;
    s->buf = malloc(s->cap);
    s->buf[0] = '\0';
    s->len = 0;
    return s;
}

void furi_string_free(FuriString* s) {
    if(!s) return;
    free(s->buf);
    free(s);
}

void furi_string_reset(FuriString* s) {
    s->len = 0;
    s->buf[0] = '\0';
}

void furi_string_cat_str(FuriString* s, const char* suffix) {
    size_t add = strlen(suffix);
    if(s->len + add + 1 > s->cap) {
        while(s->len + add + 1 > s->cap)
            s->cap *= 2;
        s->buf = realloc(s->buf, s->cap);
    }
    memcpy(s->buf + s->len, suffix, add + 1); // includes the NUL
    s->len += add;
}

const char* furi_string_get_cstr(FuriString* s) {
    return s->buf;
}
