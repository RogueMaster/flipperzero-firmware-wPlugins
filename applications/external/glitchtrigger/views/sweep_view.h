#pragma once

#include <gui/view.h>
#include "../helpers/glitch_config.h"

/* Sweep runner: fires at each pulse width from `from` to `to` in `step`
 * increments, so you can hunt for the width that faults a target. OK marks the
 * current width as a hit; Left pauses/resumes; Right restarts the sweep. */

typedef struct SweepView SweepView;

typedef enum {
    SweepViewEventMark, // OK: record current width as a suspected glitch
    SweepViewEventPause, // Left: toggle pause
    SweepViewEventRestart, // Right: restart from the beginning
} SweepViewEvent;

typedef void (*SweepViewCallback)(SweepViewEvent event, void* ctx);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

void sweep_view_set_callback(SweepView* v, SweepViewCallback cb, void* ctx);
void sweep_view_set_range(SweepView* v, uint32_t from_ns, uint32_t to_ns, uint32_t step_ns);
void sweep_view_set_progress(SweepView* v, uint32_t current_ns, uint32_t index, uint32_t total);
void sweep_view_set_counts(SweepView* v, uint32_t shots, uint32_t hits);
void sweep_view_set_running(SweepView* v, bool running);
void sweep_view_set_last_hit(SweepView* v, uint32_t hit_ns); // 0 = none yet
