#pragma once

#include <furi.h>

typedef unsigned short Unicode;

Unicode* uni_alloc(size_t size);
void uni_free(Unicode* uni);

size_t uni_decode_utf8(const char* utf8, Unicode* uni);
size_t uni_utf8_length(const char str[]);
Unicode* uni_alloc_decode_utf8(const char* utf8);

size_t uni_strlen(const Unicode* org);
Unicode* uni_strchr(register const Unicode* s, register Unicode c);
Unicode* uni_strcpy(Unicode* ret, register const Unicode* s2);
Unicode* uni_strncpy(Unicode* ret, register const Unicode* s2, register size_t n);
Unicode* uni_strcat(Unicode* ret, register const Unicode* s2);
void uni_push_back(Unicode* ret, Unicode ch);
Unicode* uni_left(register Unicode* s, size_t index);
Unicode* uni_right(register Unicode* v, size_t index);

int uni_vprintf(Unicode* s, const char format[], va_list args);
int uni_printf(Unicode* s, const char format[], ...);
int uni_cat_vprintf(Unicode* v, const char format[], va_list args);
int uni_cat_printf(Unicode* s, const char format[], ...);
