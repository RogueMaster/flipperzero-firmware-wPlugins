#include <stddef.h>

#include "glyphs.h"
#include <stratahero_icons.h>

const StrataHeroCodeGlyph glyph_left = {
    .white = &I_left_white,
    .black = &I_left_black,
    .inverse = &I_left_inverse};
const StrataHeroCodeGlyph glyph_right = {
    .white = &I_right_white,
    .black = &I_right_black,
    .inverse = &I_right_inverse};
const StrataHeroCodeGlyph glyph_up = {
    .white = &I_up_white,
    .black = &I_up_black,
    .inverse = &I_up_inverse};
const StrataHeroCodeGlyph glyph_down = {
    .white = &I_down_white,
    .black = &I_down_black,
    .inverse = &I_down_inverse};

const StrataHeroCodeGlyph* stratahero_get_code_glyph(char letter) {
    switch(letter) {
    case 'L':
        return &glyph_left;
    case 'R':
        return &glyph_right;
    case 'U':
        return &glyph_up;
    case 'D':
        return &glyph_down;
    default:
        return NULL;
    }
}
