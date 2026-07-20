#pragma once

#include <gui/view.h>
#include "../helpers/emitter_classify.h"
#include "../helpers/field_detector.h"

typedef struct SweepView SweepView;

typedef void (*SweepViewCallback)(void* context);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

/* Short OK = reset peak / contacts. */
void sweep_view_set_ok_callback(SweepView* v, SweepViewCallback cb, void* context);

/* Long OK = write the current reading to the logbook. */
void sweep_view_set_log_callback(SweepView* v, SweepViewCallback cb, void* context);

/* LEFT press = calibrate the noise floor to wherever you are standing. */
void sweep_view_set_left_callback(SweepView* v, SweepViewCallback cb, void* context);

/* Push the latest detector snapshot into the view model. */
void sweep_view_update(SweepView* v, const FieldStats* stats, const char* sens_label);

/* Briefly overprint a word in the header ("RESET", "CAL 4>7"). */
void sweep_view_flash(SweepView* v, const char* msg);

/* Advance animation phase (call on the UI tick). */
void sweep_view_tick(SweepView* v);
