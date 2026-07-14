#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <gui/canvas.h>

// u8g2 fonts (public domain, X11 misc-fixed) with Latin + Cyrillic glyphs, for
// rendering Russian via canvas_set_custom_u8g2_font(). Stock fonts are Latin-only.
extern const uint8_t pocketlab_font_6x13B[]; // bold
extern const uint8_t pocketlab_font_6x12[]; // compact

/** Bundled u8g2 font, bold or regular. */
const uint8_t* pocketlab_font(bool bold);

/** Pick the text font globally: false = stock Flipper fonts (Latin-only, native
 * look), true = the bundled Universal u8g2 font (needed for Cyrillic/Spanish).
 * Call at startup and whenever the Font setting changes. */
void pocketlab_font_set_universal(bool universal);

/** True if the Universal font is currently selected. */
bool pocketlab_font_is_universal(void);

/** Set the current font on the canvas honouring the Font setting: the stock
 * FontPrimary/FontSecondary, or the Universal u8g2 font (bold/regular). */
void pocketlab_font_apply(Canvas* canvas, bool bold);

/** Like pocketlab_font_apply but a size smaller, for tight spots such as the
 * menu tiles: stock FontSecondary, or the compact 6x12 Universal font. */
void pocketlab_font_apply_small(Canvas* canvas);
