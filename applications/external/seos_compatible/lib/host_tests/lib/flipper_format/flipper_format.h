#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <furi.h>

/* Host stand-in for the Flipper key-value file reader.
 *
 * Reproduces the lookup behaviour that matters: the search runs forward from
 * the cursor and never rewinds, and a miss leaves the cursor at end of file,
 * after which every later read on the same handle fails.
 *
 * A mock that found keys in any order would accept code the device rejects.
 */

typedef struct FlipperFormat FlipperFormat;

/* Builds a reader over the text of a file. */
FlipperFormat* flipper_format_string_alloc_from(const char* contents);
void flipper_format_free(FlipperFormat* format);

/* Cursor position, for tests that check a miss consumed the rest. */
size_t flipper_format_mock_cursor(const FlipperFormat* format);
bool flipper_format_mock_at_end(const FlipperFormat* format);

bool flipper_format_read_header(FlipperFormat* format, FuriString* name, uint32_t* version);
bool flipper_format_read_uint32(
    FlipperFormat* format,
    const char* key,
    uint32_t* data,
    const uint16_t count);
bool flipper_format_read_hex(
    FlipperFormat* format,
    const char* key,
    uint8_t* data,
    const uint16_t count);
bool flipper_format_rewind(FlipperFormat* format);
