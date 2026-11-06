/* Text and pixel helpers of the Flipper screens that need no SDK, so that
 * they are tested on the host: fitting and wrapping text to a width (the
 * caller measures with the canvas font), UIDs, the colour swatch's
 * luminance and Bayer dithering, tag type and spool names.
 *
 * The firmware fonts are ASCII only, so the texts are too ("..." for an
 * ellipsis). Buffers get at most cap - 1 characters and a NUL.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tag_dump.h"

/* The width of `text` in pixels with the font the caller draws in. */
typedef uint16_t (*UiMeasure)(void* ctx, const char* text);

/* Cuts `text` in place so that it measures at most max_width, ending in
 * "..." when that fits in the original string, else just cut. */
void ui_text_fit(char* text, uint16_t max_width, UiMeasure measure, void* ctx);

/* The next line of `text` from *pos: up to a '\n', else as many words as
 * fit max_width and the buffer (a word too long for a line is cut).
 * Advances *pos past the line and the spaces after a soft break; false at
 * the end of the text. */
bool ui_text_wrap(
    const char* text,
    size_t* pos,
    char* out,
    size_t cap,
    uint16_t max_width,
    UiMeasure measure,
    void* ctx);

/* Hex, the whole UID up to four bytes, else "04B7...3C80". */
size_t ui_uid_short(const uint8_t* uid, size_t len, char* out, size_t cap);

/* "91:B4:E7:C2". */
size_t ui_uid_colons(const uint8_t* uid, size_t len, char* out, size_t cap);

/* 0 (black) .. 255 (white) of a 0xRRGGBBAA colour (Rec. 601 weights). */
uint8_t ui_luminance(uint32_t rgba);

/* Whether the pixel at (x, y) is ink in a 4x4 ordered dithering of a
 * colour with this luminance. */
bool ui_bayer_ink(int x, int y, uint8_t luminance);

/* "MIFARE Classic 1K" / "MIFARE Classic 4K" by SAK; "NTAG213" / "NTAG215" /
 * "NTAG216" by page count, else "NTAG". */
const char* ui_tag_type_name(const TagDump* dump);

/* A spool's title: "material subtype", or the subtype alone when it starts
 * with the material as a word ("PLA Silk+"). */
size_t ui_spool_name(const char* material, const char* subtype, char* out, size_t cap);
