/* SPDX-License-Identifier: GPL-3.0-only */
#include "cstr.h"

#include <ctype.h>
#include <string.h>

bool cstr_has_any(const char* s, const char* set) {
    for(; *s; s++)
        if(strchr(set, *s)) return true;
    return false;
}

char* cstr_next_word(char* s, char** save) {
    char* p = s ? s : *save;
    if(!p) return NULL;
    while(*p == ' ')
        p++;
    if(!*p) {
        *save = p;
        return NULL;
    }
    char* start = p;
    while(*p && *p != ' ')
        p++;
    if(*p) *p++ = '\0';
    *save = p;
    return start;
}

static int ci_cmp(const char* a, const char* b) {
    for(;; a++, b++) {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if(ca != cb || !ca) return ca - cb;
    }
}

void cstr_sort(char* base, size_t count, size_t stride) {
    char tmp[128];
    if(stride > sizeof(tmp)) return;
    for(size_t i = 1; i < count; i++) {
        memcpy(tmp, base + i * stride, stride);
        size_t j = i;
        while(j > 0 && ci_cmp(base + (j - 1) * stride, tmp) > 0) {
            memcpy(base + j * stride, base + (j - 1) * stride, stride);
            j--;
        }
        memcpy(base + j * stride, tmp, stride);
    }
}

unsigned long cstr_to_ulong(const char* s, unsigned long max) {
    while(*s == ' ')
        s++;
    unsigned long v = 0;
    while(*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned long)(*s - '0');
        if(v > max) return max;
        s++;
    }
    return v;
}
