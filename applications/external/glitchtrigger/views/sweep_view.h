#pragma once

#include <gui/view.h>
#include "../helpers/glitch_config.h"

/* Sweep runner: fires across a width range (optionally a delay x width grid),
 * so you can hunt for the parameters that fault a target. OK marks the current
 * point as a hit; Left pauses/resumes; Right restarts. With Auto-hit on, the
 * feedback pin marks hits for you. */

typedef struct SweepView SweepView;

typedef enum {
    SweepViewEventMark, // OK: record current point as a suspected glitch
    SweepViewEventPause, // Left: toggle pause
    SweepViewEventRestart, // Right: restart from the beginning
} SweepViewEvent;

typedef void (*SweepViewCallback)(SweepViewEvent event, void* ctx);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

void sweep_view_set_callback(SweepView* v, SweepViewCallback cb, void* ctx);
void sweep_view_set_width_range(SweepView* v, uint32_t from_ns, uint32_t to_ns);
void sweep_view_set_point(SweepView* v, uint32_t width_ns, uint32_t delay_us, bool is_2d);
void sweep_view_set_progress(SweepView* v, uint32_t index, uint32_t total);
void sweep_view_set_counts(SweepView* v, uint32_t shots, uint32_t hits);
void sweep_view_set_running(SweepView* v, bool running);
void sweep_view_set_auto(SweepView* v, bool auto_on);
void sweep_view_set_last_hit(SweepView* v, uint32_t hit_ns, uint32_t hit_delay_us);
