#pragma once

#include <gui/view.h>
#include "../helpers/sessionstats.h"

typedef struct SummaryView SummaryView;

SummaryView* summary_view_alloc(void);
void summary_view_free(SummaryView* sv);
View* summary_view_get_view(SummaryView* sv);

void summary_view_set(SummaryView* sv, const SessionStats* stats);
