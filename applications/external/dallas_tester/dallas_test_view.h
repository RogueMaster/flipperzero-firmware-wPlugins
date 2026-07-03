#pragma once

#include <gui/view.h>
#include "dallas_test_worker.h"

typedef struct DallasTestView DallasTestView;

DallasTestView* dallas_test_view_alloc(void);
void dallas_test_view_free(DallasTestView* view);
View* dallas_test_view_get_view(DallasTestView* view);

// Publishes the latest measurement (called from the worker thread; model lock makes it safe).
void dallas_test_view_set_result(DallasTestView* view, const DallasTestResult* result);

// Clears the display back to the "apply a key" prompt.
void dallas_test_view_reset(DallasTestView* view);
