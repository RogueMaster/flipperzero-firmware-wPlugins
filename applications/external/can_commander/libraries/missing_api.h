#pragma once

#include <stddef.h>

char* local_strtok_r(char* s, const char* delim, char** save_ptr);
size_t local_strnlen(const char* s, size_t maxlen);
