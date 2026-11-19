#pragma once
#include <stddef.h>
typedef struct {
    const char* category;
    const char* name;
    const char* ps5;
    const char* xbox;
    const char* effect;
    const char* unlock;
    const char* status;
} GtaCheat;
extern const GtaCheat gta_cheat_catalog[];
extern const size_t gta_cheat_count;
/* Titles from supplied image. NOT verified GTA VI codes. */
extern const char* const gta_image_claims[];
extern const size_t gta_image_claim_count;
