#pragma once

typedef struct Icon Icon;

typedef struct {
    const Icon* white;
    const Icon* black;
    const Icon* inverse;
} StrataHeroCodeGlyph;

const StrataHeroCodeGlyph* stratahero_get_code_glyph(char letter);
