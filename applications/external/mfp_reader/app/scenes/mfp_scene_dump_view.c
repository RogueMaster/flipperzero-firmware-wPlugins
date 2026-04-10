#include "../mfp_app.h"
#include "../mfp_keys.h"
#include "mfp_scene_config.h"

#include <gui/scene_manager.h>
#include <gui/modules/text_box.h>
#include <furi.h>

/* Render the current in-memory dump as raw hex byte pairs. Format
 * mirrors the official Flipper NFC app's mf_classic dump view:
 * "XXXX XXXX XXXX XXXX" with no per-block prefixes. The TextBox
 * widget with TextBoxFontHex uses a proper monospace font and
 * handles wrapping + scrolling out of the box. */
void mfp_scene_dump_view_on_enter(void* ctx) {
    MfpApp* app = ctx;

    furi_string_reset(app->text_box_store);

    uint8_t total = mfp_sector_count(app->version.size);
    for(uint8_t s = 0; s < total; s++) {
        const MfpSectorResult* r = &app->sector_results[s];
        if(r->status != MfpSectorOk) continue;

        uint8_t first = mfp_sector_first_block(app->version.size, s);
        uint8_t blocks = mfp_sector_block_count(app->version.size, s);
        for(uint8_t b = 0; b < blocks; b++) {
            const uint8_t* data = app->blocks[first + b];
            /* 16 bytes as 8 two-byte groups separated by spaces, same
             * as nfc_render_mf_classic_dump in the stock firmware. */
            for(uint8_t i = 0; i < MFP_BLOCK_SIZE; i += 2) {
                furi_string_cat_printf(
                    app->text_box_store, "%02X%02X ", data[i], data[i + 1]);
            }
            /* Line break after each block so blocks within a sector
             * are at least visually stacked. */
            furi_string_cat_str(app->text_box_store, "\n");
        }
        /* Extra blank line between sectors. */
        furi_string_cat_str(app->text_box_store, "\n");
    }

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontHex);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));

    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewTextBox);
}

bool mfp_scene_dump_view_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void mfp_scene_dump_view_on_exit(void* ctx) {
    MfpApp* app = ctx;
    text_box_reset(app->text_box);
}
