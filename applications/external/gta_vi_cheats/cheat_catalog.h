#pragma once

#include <stddef.h>

typedef struct {
    const char* category;
    const char* name;
    const char* ps5;
    const char* xbox;
    const char* effect;
    const char* status;
} GtaCheat;

extern const GtaCheat gta_cheat_catalog[];
extern const size_t gta_cheat_count;
