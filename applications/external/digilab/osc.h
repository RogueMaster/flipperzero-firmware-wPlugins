/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
#ifndef _OSC_H
#define _OSC_H
#include <ringbuffer/ringbuffer.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <lib/toolbox/value_index.h>
#include "app_params.h"

typedef enum {
    TRIG_DOWN,
    TRIG_UP,
    TRIG_BOTH
} OscWindowTrigger;

#define OSC_WINDOW_INDENT_L 10
#define OSC_WINDOW_INDENT_R 5
typedef struct {
    struct {
        uint8_t x;
        uint8_t y;
        uint8_t w;
        uint8_t h;
    } rect;

    double vDiv;
    double tDiv;
    double vMax;
    double vMin;
    double vAvg;
    OscWindowTrigger vTrigType;
    float vTrig;
    float vTrigX;
    uint16_t windowStart;
    uint16_t windowStop;

    RingBuffer* samples;
    size_t sampleSize;
} OscWindow;
int OscWindow_draw(Canvas* canvas, OscWindow* oscWindow);
OscWindow* OscWindow_create(size_t sampleCount, uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void OscWindow_add(OscWindow* oscWindow, uint32_t value);
uint32_t OscWindow_get(OscWindow* oscWindow, size_t idx);
void OscWindow_free(OscWindow* oscWindow);
size_t OscWindow_findTrigger(
    OscWindow* oscWindow,
    OscWindowTrigger trigger,
    uint32_t vTrig,
    size_t trigOffset);

#endif /* end of include guard: _OSC_H */
