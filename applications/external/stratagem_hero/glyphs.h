#pragma once

#define CODE_GLYPH_WIDTH  12
#define CODE_GLYPH_HEIGHT 12

typedef struct Icon Icon;

typedef struct {
    const Icon* white;
    const Icon* black;
    const Icon* inverse;
} StrataHeroCodeGlyph;

const StrataHeroCodeGlyph* stratahero_get_code_glyph(char letter);
