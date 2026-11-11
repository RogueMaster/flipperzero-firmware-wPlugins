/* Minimal stand-in for the Flipper SDK so app code can run in host tests */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x)   (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

void shim_check_failed(const char* file, int line);
#define furi_check(condition)                                   \
    do {                                                        \
        if(!(condition)) shim_check_failed(__FILE__, __LINE__); \
    } while(0)
#define furi_assert(condition) furi_check(condition)

/* Flipper's allocator returns zeroed memory */
#define malloc(size) calloc(1, (size))

size_t shim_strlcpy(char* dest, const char* src, size_t size);
#define strlcpy shim_strlcpy
