#include "unicode_utils.h"

Unicode* uni_alloc(size_t size) {
    return malloc((size + 1) * sizeof(Unicode));
}

void uni_free(Unicode* uni) {
    free(uni);
}

static void uni_parse_utf8_byte(char c, uint8_t* state, Unicode* encoding) {
    if(c == '\0') {
        *state = 0;
        *encoding = 0;
        return;
    }
    if(*state == 0) {
        if(c >= 0xfc) {
            *state = 5;
            c &= 1;
        } else if(c >= 0xf8) {
            *state = 4;
            c &= 3;
        } else if(c >= 0xf0) {
            *state = 3;
            c &= 7;
        } else if(c >= 0xe0) {
            *state = 2;
            c &= 15;
        } else if(c >= 0xc0) {
            *state = 1;
            c &= 0x01f;
        }
        *encoding = c;
    } else {
        (*state)--;
        *encoding <<= 6;
        c &= 0x03f;
        *encoding |= c;
    }
}

size_t uni_utf8_length(const char str[]) {
    size_t size = 0;
    uint8_t s = 0;
    Unicode u = 0;
    while(*str) {
        uni_parse_utf8_byte(*str, &s, &u);
        size += (s == 0);
        str++;
    }
    return size;
}

size_t uni_decode_utf8(const char* utf8, Unicode* uni) {
    size_t size = 0;
    uint8_t s = 0;
    Unicode u = 0;
    while(*utf8) {
        uni_parse_utf8_byte(*utf8, &s, &u);
        if(s == 0) {
            uni[size++] = u;
        }
        utf8++;
    }
    uni[size] = '\0';

    return size;
}

Unicode* uni_alloc_decode_utf8(const char* utf8) {
    size_t len = uni_utf8_length(utf8);
    if(!len) return NULL;
    Unicode* uni = uni_alloc(len);
    uni_decode_utf8(utf8, uni);
    return uni;
}

size_t uni_strlen(const Unicode* org) {
    register const Unicode* s = org;

    while(*s++) /* EMPTY */
        ;

    return --s - org;
}

Unicode* uni_strchr(register const Unicode* s, register Unicode c) {
    c = (Unicode)c;

    while(c != *s) {
        if(*s++ == '\0') return NULL;
    }
    return (Unicode*)s;
}

Unicode* uni_strcpy(Unicode* ret, register const Unicode* s2) {
    register Unicode* s1 = ret;

    while((*s1++ = *s2++) != '\0') /* EMPTY */
        ;

    return ret;
}

Unicode* uni_strncpy(Unicode* ret, register const Unicode* s2, register size_t n) {
    register Unicode* s1 = ret;

    if(n > 0) {
        while((*s1++ = *s2++) && --n > 0) /* EMPTY */
            ;
        if((*--s2 == '\0') && --n > 0) {
            do {
                *s1++ = '\0';
            } while(--n > 0);
        } else
            *s1++ = '\0';
    }
    return ret;
}

Unicode* uni_strcat(Unicode* ret, register const Unicode* s2) {
    register Unicode* s1 = ret;
    while(*s1++ != '\0') /* EMPTY */
        ;
    s1--;
    while((*s1++ = *s2++) != '\0') /* EMPTY */
        ;
    return ret;
}

void uni_push_back(Unicode* ret, Unicode ch) {
    register Unicode* s1 = ret;
    while(*s1++ != '\0') /* EMPTY */
        ;
    s1--;
    *s1++ = ch;
    *s1++ = '\0';
}

Unicode* uni_left(register Unicode* v, size_t index) {
    const size_t size = uni_strlen(v);
    if(index > size) return NULL;
    v[index] = '\0';
    return v;
}

Unicode* uni_right(register Unicode* v, size_t index) {
    const size_t size = uni_strlen(v);
    if(index > size) return NULL;
    Unicode* v1 = v + index;

    while((*v++ = *v1++) != '\0')
        ;
    *v = '\0';
    return v;
}

int uni_vprintf(Unicode* s, const char format[], va_list args) {
    FuriString* string = furi_string_alloc();
    int result = furi_string_vprintf(string, format, args);
    uni_decode_utf8(furi_string_get_cstr(string), s);
    furi_string_free(string);
    return result;
}

int uni_printf(Unicode* s, const char format[], ...) {
    va_list args;
    va_start(args, format);
    int result = uni_vprintf(s, format, args);
    va_end(args);
    return result;
}

int uni_cat_vprintf(Unicode* v, const char format[], va_list args) {
    FuriString* string = furi_string_alloc();
    Unicode* temp = malloc(50);
    int ret = furi_string_vprintf(string, format, args);
    uni_decode_utf8(furi_string_get_cstr(string), temp);
    uni_strcat(v, temp);
    furi_string_free(string);
    free(temp);
    return ret;
}

int uni_cat_printf(Unicode* s, const char format[], ...) {
    va_list args;
    va_start(args, format);
    int result = uni_cat_vprintf(s, format, args);
    va_end(args);
    return result;
}
