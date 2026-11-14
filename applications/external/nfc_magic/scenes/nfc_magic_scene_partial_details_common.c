#include "nfc_magic_scene_partial_details_common.h"

void nfc_magic_partial_details_append_indices(
    FuriString* out,
    const uint8_t* bitmap,
    uint16_t count,
    uint16_t max_shown) {
    uint16_t shown = 0;
    for(uint16_t i = 0; i < count; i++) {
        if(bitmap[i >> 3] & (1u << (i & 7u))) {
            if(max_shown > 0 && shown >= max_shown) {
                furi_string_cat_str(out, "...");
                return;
            }
            furi_string_cat_printf(out, "%u ", i);
            shown++;
        }
    }
}

bool nfc_magic_partial_details_any_index(const uint8_t* bitmap, uint16_t count) {
    for(uint16_t i = 0; i < count; i++) {
        if(bitmap[i >> 3] & (1u << (i & 7u))) return true;
    }
    return false;
}
