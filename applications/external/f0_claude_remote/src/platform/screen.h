#pragma once

#include <furi.h>

#include "../core/action.h"
#include "../core/link.h"
#include "../core/mode.h"

typedef struct Screen Screen;

Screen* screen_alloc(FuriEventLoop* event_loop, FuriMessageQueue* input_queue, Mode mode);

void screen_free(Screen* screen);

void screen_set_mode(Screen* screen, Mode mode);

void screen_set_link(Screen* screen, Link link);

void screen_flash(Screen* screen, Action action, uint32_t duration_ms);
