#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include "helpers/tracker_db.h"

typedef struct AlertView AlertView;
typedef void (*AlertViewCallback)(void* context);

AlertView* alert_view_alloc(void);
void alert_view_free(AlertView* av);
View* alert_view_get_view(AlertView* av);

/** @p demo stamps the whole screen SIMULATED. */
void alert_view_set_record(AlertView* av, const TrackerRecord* rec, bool demo);
void alert_view_tick(AlertView* av); /* drives the strobe */
void alert_view_set_ok_callback(AlertView* av, AlertViewCallback cb, void* context);
