#pragma once

#include <gui/view.h>
#include "../helpers/field_detector.h"

typedef struct SweepView SweepView;

typedef void (*SweepViewCallback)(void* context);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

/* OK press = reset peak / contacts. */
void sweep_view_set_ok_callback(SweepView* v, SweepViewCallback cb, void* context);

/* Push the latest detector snapshot into the view model. */
void sweep_view_update(SweepView* v, const FieldStats* stats, const char* sens_label);

/* Advance animation phase (call on the UI tick). */
void sweep_view_tick(SweepView* v);
