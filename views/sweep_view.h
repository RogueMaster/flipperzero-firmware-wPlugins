#pragma once

#include <gui/view.h>
#include "../helpers/ir_sense.h"

typedef struct SweepView SweepView;
typedef void (*SweepViewCallback)(void* context);

SweepView* sweep_view_alloc(void);
void sweep_view_free(SweepView* v);
View* sweep_view_get_view(SweepView* v);

void sweep_view_set_ok_callback(SweepView* v, SweepViewCallback cb, void* context);
void sweep_view_set_long_ok_callback(SweepView* v, SweepViewCallback cb, void* context);

void sweep_view_update(SweepView* v, const IrStats* stats);
void sweep_view_tick(SweepView* v);
