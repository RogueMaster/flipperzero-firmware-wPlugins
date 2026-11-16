#pragma once

#include <furi.h>

#include "../core/action.h"
#include "../ui/view_state.h"

typedef struct Screen Screen;

Screen*
    screen_alloc(FuriEventLoop* event_loop, FuriMessageQueue* input_queue, const ViewState* state);

void screen_free(Screen* screen);

ViewState screen_state(Screen* screen);

void screen_set_state(Screen* screen, const ViewState* state);

void screen_confirm(Screen* screen, Action action, uint32_t flash_ms, uint32_t sent_ms);
