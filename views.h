#pragma once

#include "nfc_canary_i.h"

typedef enum {
    ScreenIntro = 0, /* first-run splash: tag vs reader */
    ScreenStatus, /* home: armed state, tier, history strip */
    ScreenEvents, /* scrollable event log */
    ScreenEventDetail, /* one event, expanded */
    ScreenSession, /* session summary */
    ScreenSettings,
    ScreenDiag, /* raw hardware, kept for testing */
} Screen;

typedef struct {
    Screen screen;
    uint16_t event_cursor; /* index into event log, 0 = newest */
    uint8_t settings_cursor;
    uint8_t settings_top; /* first visible settings row */
} ViewState;

/* Draw the current screen. Called from the GUI thread. */
void views_draw(
    Canvas* canvas,
    const ViewState* view,
    AlerterState* state,
    const AlerterSettings* settings);
