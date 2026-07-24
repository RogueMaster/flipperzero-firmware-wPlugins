#include "mf_big_callsign_font.h"

#include "mf_callsign_gen.h"

static const uint8_t mf_big_glyphs[36][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43},
};

static const uint8_t* mf_big_glyph(char ch) {
    if(ch >= '0' && ch <= '9') return mf_big_glyphs[ch - '0'];
    if(ch >= 'A' && ch <= 'Z') return mf_big_glyphs[10U + (uint8_t)(ch - 'A')];
    return NULL;
}

void mf_big_callsign_draw_char(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    char ch,
    bool inverted) {
    const uint8_t* glyph = mf_big_glyph(ch);

    if(inverted) {
        canvas_draw_box(canvas, x - 1, y - 1, MF_BIG_CALLSIGN_WIDTH + 2U, MF_BIG_CALLSIGN_HEIGHT + 2U);
        canvas_set_color(canvas, ColorWhite);
    }
    if(glyph != NULL) {
        for(uint8_t col = 0U; col < 5U; col++) {
            for(uint8_t row = 0U; row < 7U; row++) {
                if((glyph[col] & (1U << row)) != 0U) {
                    canvas_draw_box(
                        canvas,
                        x + (int32_t)(col * MF_BIG_CALLSIGN_SCALE),
                        y + (int32_t)(row * MF_BIG_CALLSIGN_SCALE),
                        MF_BIG_CALLSIGN_SCALE,
                        MF_BIG_CALLSIGN_SCALE);
                }
            }
        }
    } else if(ch == '_') {
        canvas_draw_box(
            canvas,
            x,
            y + MF_BIG_CALLSIGN_HEIGHT - MF_BIG_CALLSIGN_SCALE,
            MF_BIG_CALLSIGN_WIDTH,
            MF_BIG_CALLSIGN_SCALE);
    }
    if(inverted) canvas_set_color(canvas, ColorBlack);
}

void mf_big_callsign_draw_text(
    Canvas* canvas,
    const char* text,
    const char* target,
    uint8_t length,
    int32_t y,
    bool mark_errors) {
    uint8_t count = 0U;
    uint8_t slots;
    int32_t left;

    if(canvas == NULL || text == NULL || length == 0U) return;
    slots = length > MF_CALLSIGN_MAX_LEN ? MF_CALLSIGN_MAX_LEN : length;
    left = (128 - ((int32_t)slots * MF_BIG_CALLSIGN_WIDTH +
                  ((int32_t)slots - 1) * MF_BIG_CALLSIGN_GAP)) /
           2;
    while(count < slots && text[count] != '\0') count++;
    for(uint8_t i = 0U; i < slots; i++) {
        bool mismatch =
            mark_errors && (i >= count || target == NULL || text[i] != target[i]);
        mf_big_callsign_draw_char(
            canvas,
            left + (int32_t)i * (MF_BIG_CALLSIGN_WIDTH + MF_BIG_CALLSIGN_GAP),
            y,
            i < count ? text[i] : '\0',
            mismatch);
    }
}
