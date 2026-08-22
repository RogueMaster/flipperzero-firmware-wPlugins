#pragma once

#include <gui/view.h>
#include "chord_db.h"

typedef struct PracticeView PracticeView;

typedef enum {
    PracticeActionPrev, // Left:  step back one chord (and pause)
    PracticeActionNext, // Right: step on one chord (and pause)
    PracticeActionFaster, // Up
    PracticeActionSlower, // Down
    PracticeActionToggle, // OK:    play / pause
} PracticeAction;

typedef void (*PracticeActionCallback)(void* context, PracticeAction action);

PracticeView* practice_view_alloc(void);
void practice_view_free(PracticeView* pv);
View* practice_view_get_view(PracticeView* pv);

void practice_view_set_action_callback(PracticeView* pv, PracticeActionCallback cb, void* context);

/** Push the whole practice state into the model in one shot. */
void practice_view_update(
    PracticeView* pv,
    const Chord* chord, // NULL when the name isn't in the library
    const char* chord_name,
    uint8_t step,
    uint8_t count,
    uint16_t bpm,
    bool playing);
