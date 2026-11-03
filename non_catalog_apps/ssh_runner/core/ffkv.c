/* SPDX-License-Identifier: GPL-3.0-only */
#include "ffkv.h"
#include "cstr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* trim(char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    char* e = s + strlen(s);
    while(e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}

void ffkv_parse(Ffkv* kv, char* text) {
    memset(kv, 0, sizeof(*kv));
    char* line = text;
    while(line && *line) {
        char* nl = strchr(line, '\n');
        if(nl) *nl = '\0';
        char* l = trim(line);
        if(*l && *l != '#') {
            char* colon = strchr(l, ':');
            if(colon) {
                *colon = '\0';
                char* key = trim(l);
                char* value = trim(colon + 1);
                if(*key) {
                    if(kv->count < FFKV_MAX_PAIRS) {
                        kv->pairs[kv->count].key = key;
                        kv->pairs[kv->count].value = value;
                        kv->count++;
                    } else {
                        kv->overflow = true;
                    }
                }
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

const char* ffkv_get_nth(const Ffkv* kv, const char* key, size_t n) {
    for(size_t i = 0; i < kv->count; i++) {
        if(strcmp(kv->pairs[i].key, key) == 0) {
            if(n == 0) return kv->pairs[i].value;
            n--;
        }
    }
    return NULL;
}

const char* ffkv_get(const Ffkv* kv, const char* key) {
    return ffkv_get_nth(kv, key, 0);
}

size_t ffkv_count(const Ffkv* kv, const char* key) {
    size_t c = 0;
    for(size_t i = 0; i < kv->count; i++)
        if(strcmp(kv->pairs[i].key, key) == 0) c++;
    return c;
}

bool ffkv_check_header(const Ffkv* kv, const char* filetype, uint32_t max_version) {
    const char* ft = ffkv_get(kv, "Filetype");
    const char* ver = ffkv_get(kv, "Version");
    if(!ft || !ver || strcmp(ft, filetype) != 0) return false;
    char* end = NULL;
    unsigned long v = strtoul(ver, &end, 10);
    if(end == ver || *end != '\0') return false;
    return v >= 1 && v <= max_version;
}

void ffkv_writer_init(FfkvWriter* w, char* buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
    w->err = cap == 0;
    if(cap) buf[0] = '\0';
}

static void put(FfkvWriter* w, const char* s) {
    if(w->err) return;
    size_t n = strlen(s);
    if(w->len + n + 1 > w->cap) {
        w->err = true;
        return;
    }
    memcpy(&w->buf[w->len], s, n + 1);
    w->len += n;
}

void ffkv_write_header(FfkvWriter* w, const char* filetype, uint32_t version) {
    ffkv_write(w, "Filetype", filetype);
    ffkv_write_u32(w, "Version", version);
}

void ffkv_write(FfkvWriter* w, const char* key, const char* value) {
    if(!value) value = "";
    if(cstr_has_any(value, "\r\n") || cstr_has_any(key, ":\r\n")) {
        w->err = true;
        return;
    }
    put(w, key);
    put(w, ": ");
    put(w, value);
    put(w, "\n");
}

void ffkv_write_u32(FfkvWriter* w, const char* key, uint32_t value) {
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)value);
    ffkv_write(w, key, tmp);
}

void ffkv_write_comment(FfkvWriter* w, const char* text) {
    put(w, "# ");
    put(w, text);
    put(w, "\n");
}
