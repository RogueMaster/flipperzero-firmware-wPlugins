#pragma once

#include <furi.h>
#include <gui/gui.h>

// Total pages in the info/options pager. Left/Right cycle through them.
#define INFO_PAGE_COUNT 2

// Which top-level screen the app is currently showing.
typedef enum {
    ScreenClock, // the normal dial + digital readout
    ScreenInfo, // the info/options pager, reached by holding OK in Set mode
} AppScreen;

// Renders one page of the info/options pager, plus the page-nav arrows at the screen edges.
void draw_info_screen(Canvas* canvas, uint8_t page);
