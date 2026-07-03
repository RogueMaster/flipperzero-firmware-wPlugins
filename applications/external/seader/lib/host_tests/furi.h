#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define FURI_LOG_D(...) \
    do {                \
    } while(0)
#define FURI_LOG_I(...) \
    do {                \
    } while(0)
#define FURI_LOG_W(...) \
    do {                \
    } while(0)
#define FURI_LOG_E(...) \
    do {                \
    } while(0)

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} FuriString;

static inline FuriString* furi_string_alloc(void) {
    FuriString* string = calloc(1, sizeof(FuriString));
    if(!string) {
        return NULL;
    }

    string->cap = 1U;
    string->data = calloc(string->cap, sizeof(char));
    if(!string->data) {
        free(string);
        return NULL;
    }

    return string;
}

static inline void furi_string_free(FuriString* string) {
    if(!string) {
        return;
    }

    free(string->data);
    free(string);
}

static inline const char* furi_string_get_cstr(const FuriString* string) {
    return (string && string->data) ? string->data : "";
}

static inline size_t furi_string_size(const FuriString* string) {
    return string ? string->len : 0U;
}

static inline void furi_string_reset(FuriString* string) {
    if(!string || !string->data) {
        return;
    }

    string->len = 0U;
    string->data[0] = '\0';
}

static inline bool furi_string_reserve(FuriString* string, size_t needed_len) {
    if(!string) {
        return false;
    }

    if(needed_len + 1U <= string->cap) {
        return true;
    }

    size_t new_cap = string->cap ? string->cap : 1U;
    while(new_cap < needed_len + 1U) {
        new_cap *= 2U;
    }

    char* new_data = realloc(string->data, new_cap);
    if(!new_data) {
        return false;
    }

    string->data = new_data;
    string->cap = new_cap;
    return true;
}

static inline void furi_string_set_str(FuriString* string, const char* value) {
    if(!string) {
        return;
    }

    const char* src = value ? value : "";
    size_t len = strlen(src);
    if(!furi_string_reserve(string, len)) {
        return;
    }

    memcpy(string->data, src, len + 1U);
    string->len = len;
}

static inline void furi_string_cat_printf(FuriString* string, const char* format, ...) {
    if(!string || !format) {
        return;
    }

    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if(needed < 0) {
        va_end(args);
        return;
    }

    size_t required_len = string->len + (size_t)needed;
    if(!furi_string_reserve(string, required_len)) {
        va_end(args);
        return;
    }

    vsnprintf(string->data + string->len, string->cap - string->len, format, args);
    va_end(args);
    string->len = required_len;
}
