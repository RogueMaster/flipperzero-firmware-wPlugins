/* Bambu's colour codes and English names (`11803`, `Caramel`), which the
 * tags do not carry, for naming a tag's colour without the app. The table
 * (bambu_color_table.h) is generated from tools/data/bambu_color_codes.json
 * by tools/fixtures/make_color_codes.py, as the host's BambuColorCodes is,
 * and is looked up the same way.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t code; /* as printed on the box, 11803 */
    const char* name; /* static, ASCII */
} BambuColor;

/* By filament id (block 1, "GFA01") and the colour part of the variant id
 * ("A01-N3" -> "N3"), else by filament id and the RGB of rgba; the first
 * row that matches. False when none does or filament_id is empty. */
bool bambu_color_find(
    const char* filament_id,
    const char* variant_id,
    bool has_rgba,
    uint32_t rgba,
    BambuColor* out);
