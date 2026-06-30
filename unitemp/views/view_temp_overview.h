#pragma once

#include <gui/view.h>

typedef struct TempOverview TempOverview;

TempOverview* temp_overview_alloc(void* context);

void temp_overview_free(TempOverview* temp_overview);

View* temp_overview_get_view(TempOverview* temp_overview);

void temp_overview_refresh_data(TempOverview* instance);
