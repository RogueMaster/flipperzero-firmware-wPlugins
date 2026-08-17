#pragma once

#include <stdbool.h>

#include <gui/canvas.h>

#define MF_BIG_CALLSIGN_SCALE  3U
#define MF_BIG_CALLSIGN_WIDTH  15U
#define MF_BIG_CALLSIGN_HEIGHT 21U
#define MF_BIG_CALLSIGN_GAP    3U

void mf_big_callsign_draw_char(Canvas* canvas, int32_t x, int32_t y, char ch, bool inverted);
void mf_big_callsign_draw_text(
    Canvas* canvas,
    const char* text,
    const char* target,
    uint8_t length,
    int32_t y,
    bool mark_errors);
