/* Storage-backed Terminus 24 prompt glyphs.  The canonical pixels live in
 * morse_flipper_terminus24_source.h and are packed into assets/terminus24.bin. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_FLIPPER_TERMINUS24_WIDTH       12U
#define MORSE_FLIPPER_TERMINUS24_HEIGHT      24U
#define MORSE_FLIPPER_TERMINUS24_PACKED_SIZE 36U
#define MORSE_FLIPPER_TERMINUS24_CACHE_SLOTS 10U

typedef struct {
    uint8_t ch;
    uint8_t rows[MORSE_FLIPPER_TERMINUS24_PACKED_SIZE];
} MorseFlipperTerminus24PreparedGlyph;

typedef struct {
    MorseFlipperTerminus24PreparedGlyph slots[MORSE_FLIPPER_TERMINUS24_CACHE_SLOTS];
    uint8_t count;
    bool asset_ok;
    bool prepared;
} MorseFlipperTerminus24Cache;

struct MorseFlipperApp;

/* Called before a ViewDispatcher update.  It is deliberately never called
 * from a draw callback: all rendered rows come from this bounded workspace. */
void morse_flipper_terminus24_prepare(struct MorseFlipperApp* app);

const MorseFlipperTerminus24PreparedGlyph*
    morse_flipper_terminus24_prepared(const MorseFlipperTerminus24Cache* cache, uint8_t ch);

uint16_t morse_flipper_terminus24_prepared_row(
    const MorseFlipperTerminus24PreparedGlyph* glyph,
    size_t row);
