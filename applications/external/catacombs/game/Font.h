#pragma once

#include <stdint.h>
#include "game/Defines.h"

#define FONT_WIDTH  4
#define FONT_HEIGHT 6

class Font {
public:
    static constexpr int glyphWidth = 4;
    static constexpr int glyphHeight = 8;
    static constexpr int firstGlyphIndex = 32;

    static void
        PrintString(const char* str, uint8_t line, uint8_t x, uint8_t colour = COLOUR_BLACK);
    static void PrintInt(uint16_t value, uint8_t line, uint8_t x, uint8_t xorMask = COLOUR_BLACK);

private:
    static void DrawChar(uint8_t* screenPtr, char c, uint8_t xorMask);
};
