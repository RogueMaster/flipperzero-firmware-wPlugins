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
/* All 28 PS5 sequences transcribed from the user-supplied screenshot.
 * main_catalog_name indicates a matching EFFECT, not a verified image input. */
typedef struct {
    const char* name;
    const char* ps5_sequence;
    const char* main_catalog_name; /* NULL for screenshot-only effects */
} GtaScreenshotCode;
extern const GtaScreenshotCode gta_screenshot_codes[];
extern const size_t gta_screenshot_count;
extern const size_t gta_unverified_count;
