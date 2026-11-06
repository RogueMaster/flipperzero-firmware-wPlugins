#include "bambu_colors.h"

#include <string.h>

typedef struct {
    uint8_t filament; /* bambu_color_filaments */
    uint8_t name; /* bambu_color_name_at */
    char color[4]; /* colour id, NUL-padded */
    uint8_t rgb[3];
    uint8_t code[3]; /* little endian */
} BambuColorRow;

#include "bambu_color_table.h"

_Static_assert(sizeof(BambuColorRow) == 12, "a packed row");

static void take(const BambuColorRow* row, BambuColor* out) {
    out->code = row->code[0] | (uint32_t)row->code[1] << 8 | (uint32_t)row->code[2] << 16;
    out->name = bambu_color_names + bambu_color_name_at[row->name];
}

static bool same_filament(const BambuColorRow* row, const char* filament_id) {
    return strcmp(bambu_color_filaments[row->filament], filament_id) == 0;
}

bool bambu_color_find(
    const char* filament_id,
    const char* variant_id,
    bool has_rgba,
    uint32_t rgba,
    BambuColor* out) {
    if(filament_id == NULL || filament_id[0] == '\0') return false;
    const char* dash = variant_id ? strrchr(variant_id, '-') : NULL;
    if(dash != NULL && strlen(dash + 1) < sizeof(bambu_color_rows[0].color)) {
        for(unsigned i = 0; i < BAMBU_COLOR_ROWS; i++) {
            const BambuColorRow* row = &bambu_color_rows[i];
            if(same_filament(row, filament_id) &&
               strncmp(row->color, dash + 1, sizeof(row->color)) == 0) {
                take(row, out);
                return true;
            }
        }
    }
    if(!has_rgba) return false;
    for(unsigned i = 0; i < BAMBU_COLOR_ROWS; i++) {
        const BambuColorRow* row = &bambu_color_rows[i];
        if(same_filament(row, filament_id) && row->rgb[0] == (uint8_t)(rgba >> 24) &&
           row->rgb[1] == (uint8_t)(rgba >> 16) && row->rgb[2] == (uint8_t)(rgba >> 8)) {
            take(row, out);
            return true;
        }
    }
    return false;
}
