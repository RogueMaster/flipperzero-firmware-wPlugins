#pragma once

#include <gui/canvas.h>

typedef struct {
    char lines[4][80];
    char left[12];
    char center[12];
    char right[12];
    int8_t selected;
    bool companion_qr;
} ClipboardScreen;

void clipboard_screen_draw(Canvas* canvas, const ClipboardScreen* screen);
void clipboard_screen_info(ClipboardScreen* screen, bool qr);
int32_t clipboard_info_app(void);
