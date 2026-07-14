#pragma once
#include <gui/view.h>
#include "../ir_main.h"

typedef struct {
    char* landing;
} LandingModel;

View* landing_view_alloc(App* app);
void landing_view_free(View* landing_view);
