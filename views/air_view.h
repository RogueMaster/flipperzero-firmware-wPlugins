#pragma once

#include <gui/view.h>
#include "helpers/air_check.h"

typedef struct AirView AirView;
typedef void (*AirViewCallback)(void* context);

AirView* air_view_alloc(void);
void air_view_free(AirView* av);
View* air_view_get_view(AirView* av);

void air_view_set_snapshot(AirView* av, const AirSnapshot* snap);

/** Left opens Help & About, which explains what this can and cannot see. */
void air_view_set_help_callback(AirView* av, AirViewCallback cb, void* context);
