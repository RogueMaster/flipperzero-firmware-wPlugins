#pragma once

#include <stdint.h>
#include <stddef.h>

/** Number of built-in font sizes, smallest first. */
#define READER_FONT_COUNT 4

/**
 * Font used for a given size index.
 *
 * @param size 0 .. READER_FONT_COUNT - 1
 * @returns u8g2 font data, never NULL
 */
const uint8_t* reader_font_get(size_t size);

/** Human readable name of a size index, for the status bar. */
const char* reader_font_name(size_t size);

/** Pixels below the baseline of a size index. */
size_t reader_font_descender(size_t size);

/** Index of the default size. */
#define READER_FONT_DEFAULT 1
