/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
#include <401_gui.h>
#include <osc.h>
static const char* TAG = "OSC";
#define OSC_MARGIN_L 20
#define OSC_MARGIN_R 10

int OscWindow_draw_grid(Canvas* canvas, OscWindow* oscWindow) {
    char str[32] = {0};
    double Max = oscWindow->vMax;
    double Min = oscWindow->vMin;

    // Calculate signal min, max, and trigger
    if(Max < 500) {
        Max = 500;
    }
    oscWindow->vTrig = Min + ((Max - Min) / 2);

    double hm = (Max) / (oscWindow->rect.h - 2); // Scale factor for height

    size_t triggerIndex =
        OscWindow_findTrigger(oscWindow, oscWindow->vTrigType, oscWindow->vTrig, 0);

    uint8_t x = oscWindow->rect.x;
    uint8_t y = oscWindow->rect.y;
    uint8_t v = (uint8_t)(RingBuffer_get(oscWindow->samples, (uint16_t)triggerIndex) / hm);
    uint8_t prevV = v;
    uint8_t prevX = 0;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);

    // Draw voltage level (e.g., 0V and max V)
    snprintf(str, sizeof(str), "%0.01fV", (double)(Max / 1000));
    canvas_draw_str_aligned(
        canvas, oscWindow->rect.x + OSC_MARGIN_L, oscWindow->rect.y, AlignRight, AlignTop, str);
    canvas_draw_str_aligned(
        canvas,
        oscWindow->rect.x + OSC_MARGIN_L,
        oscWindow->rect.y + oscWindow->rect.h,
        AlignRight,
        AlignBottom,
        "0V");

    // Calculate trigger level position for repeated use
    int triggerPosY = y + (oscWindow->rect.h - ((double)oscWindow->vTrig / hm));

    // Draw trigger level indicator "<t"
    canvas_draw_str_aligned(
        canvas,
        oscWindow->rect.x + oscWindow->rect.w - OSC_MARGIN_R,
        triggerPosY,
        AlignLeft,
        AlignCenter,
        "<t");

    // Draw grid at trigger level
    for(uint8_t xg = x + OSC_MARGIN_L; xg <= x + oscWindow->rect.w - OSC_MARGIN_R; xg += 3) {
        canvas_draw_dot(canvas, xg, triggerPosY);
    }

    // Draw signal waveform
    for(uint8_t xg = 0; xg < oscWindow->rect.w - OSC_MARGIN_L - OSC_MARGIN_R; xg++) {
        // Get the next sample and scale to window height
        size_t sampleIndex = triggerIndex + xg;
        v = (uint8_t)(RingBuffer_get(oscWindow->samples, sampleIndex) / hm);

        // Bound-check the signal to avoid exceeding height
        if(v > oscWindow->rect.h) {
            v = oscWindow->rect.h;
        }

        // Draw line from previous point to current point
        canvas_draw_line(
            canvas,
            x + prevX + OSC_MARGIN_L,
            y + (oscWindow->rect.h - prevV),
            oscWindow->rect.x + xg + OSC_MARGIN_L,
            y + (oscWindow->rect.h - v));

        // Update previous values for the next loop iteration
        prevV = v;
        prevX = xg;
    }

    return 0;
}

int OscWindow_draw(Canvas* canvas, OscWindow* oscWindow) {
    char text[32];

    canvas_set_color(canvas, ColorXOR);
    canvas_set_custom_u8g2_font(canvas, CUSTOM_FONT_TINY_REGULAR);

    snprintf(text, sizeof(text), "AVG:%0.1fV", (double)(oscWindow->vAvg));
    l401Gui_draw_btn(canvas, oscWindow->rect.x + 1, oscWindow->rect.y - 9, 0, true, text);
    snprintf(
        text,
        sizeof(text),
        "Vin:%0.1fV",
        (double)(RingBuffer_getLast(oscWindow->samples) / OSC_SCALE_V));
    l401Gui_draw_btn(canvas, oscWindow->rect.x + 41, oscWindow->rect.y - 9, 0, true, text);
    snprintf(text, sizeof(text), "Max:%0.1fV", (double)(oscWindow->vMax / OSC_SCALE_V));
    l401Gui_draw_btn(canvas, oscWindow->rect.x + 81, oscWindow->rect.y - 9, 0, true, text);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(
        canvas,
        oscWindow->rect.x + OSC_MARGIN_L,
        oscWindow->rect.y,
        oscWindow->rect.w - OSC_MARGIN_L - OSC_MARGIN_R,
        oscWindow->rect.h);

    OscWindow_draw_grid(canvas, oscWindow);
    // draw trace
    return 0;
}

OscWindow* OscWindow_create(size_t sampleCount, uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    OscWindow* oscwindow = (OscWindow*)malloc(sizeof(OscWindow));
    if(oscwindow == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate OscWindow struct");
        return NULL;
    }

    oscwindow->samples = RingBuffer_create(sampleCount);
    if(oscwindow->samples == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate OscWindow'sample buffer");
        return NULL;
    }

    oscwindow->rect.x = x;
    oscwindow->rect.y = y;
    oscwindow->rect.w = w;
    oscwindow->rect.h = h;

    oscwindow->vDiv = 1;
    oscwindow->tDiv = 1;
    oscwindow->vTrigType = TRIG_DOWN;
    oscwindow->vTrig = 100;
    oscwindow->vTrigX = w >> 1;
    oscwindow->windowStart = 0;
    oscwindow->windowStop = w;
    oscwindow->sampleSize = 0;

    return oscwindow;
}

void OscWindow_add(OscWindow* oscWindow, uint32_t value) {
    RingBuffer_add(oscWindow->samples, value);
}

uint32_t OscWindow_get(OscWindow* oscWindow, size_t idx) {
    return RingBuffer_get(oscWindow->samples, idx);
}

/*

  Return the index of the first trigger condition, passed trigOffset.

*/
size_t OscWindow_findTrigger(
    OscWindow* oscWindow,
    OscWindowTrigger trigger,
    uint32_t vTrig,
    size_t trigOffset) {
    if(oscWindow->samples == NULL || oscWindow->samples->buffer == NULL ||
       oscWindow->samples->count == 0) {
        return 0; // Invalid input or empty buffer
    }

    if(trigOffset >= oscWindow->samples->count) {
        return 0; // Offset is out of range
    }

    size_t startIndex = (oscWindow->samples->head + oscWindow->samples->bufferSize -
                         oscWindow->samples->count + trigOffset) %
                        oscWindow->samples->bufferSize;
    size_t currentIndex = startIndex;
    size_t previousIndex = (currentIndex == 0) ? oscWindow->samples->bufferSize - 1 :
                                                 currentIndex - 1;

    for(size_t i = trigOffset; i < oscWindow->samples->count; i++) {
        uint32_t currentSample = oscWindow->samples->buffer[currentIndex];
        uint32_t previousSample = oscWindow->samples->buffer[previousIndex];

        bool triggerDetected = false;
        switch(trigger) {
        case TRIG_DOWN:
            if(previousSample >= vTrig && currentSample < vTrig) {
                triggerDetected = true;
                continue;
            }
            break;
        case TRIG_UP:
            if(previousSample <= vTrig && currentSample > vTrig) {
                triggerDetected = true;
                continue;
            }
            break;
        case TRIG_BOTH:
            if((previousSample >= vTrig && currentSample < vTrig) ||
               (previousSample <= vTrig && currentSample > vTrig)) {
                triggerDetected = true;
                continue;
            }
            break;
        }

        if(triggerDetected) {
            return currentIndex;
        }

        previousIndex = currentIndex;
        currentIndex = (currentIndex + 1) % oscWindow->samples->bufferSize;
    }

    return 0; // Trigger not found
}

void OscWindow_free(OscWindow* oscWindow) {
    RingBuffer_free(oscWindow->samples);
    free(oscWindow);
}
