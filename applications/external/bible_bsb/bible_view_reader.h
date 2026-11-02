#pragma once

#include <gui/canvas.h>
#include <input/input.h>

#include "bible_app.h"

/* ============================================================================
 * Reader view — paginated scripture display
 * ============================================================================ */

/**
 * Draw the Reader view: header with passage ref, divider, paginated text.
 */
void bible_bsb_view_reader_draw(Canvas* canvas, void* ctx);

/**
 * Handle Reader input: Up/Down=verse jump, Left/Right=scroll, OK=ActionMenu/save range, Back=return.
 */
bool bible_bsb_view_reader_input(InputEvent* event, void* ctx);

/**
 * Open the VerseSelect range picker pre-populated with the currently visible verse
 * in the Reader, for either saving to collection or sharing via NFC.
 */
void bible_reader_open_verse_select(
    BibleAppState* state,
    SceneManager* scene_manager,
    bool for_nfc);
