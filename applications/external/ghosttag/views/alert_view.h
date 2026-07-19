#pragma once

#include <gui/view.h>
#include "helpers/tracker_db.h"

typedef struct AlertView AlertView;
typedef void (*AlertViewCallback)(void* context);

AlertView* alert_view_alloc(void);
void alert_view_free(AlertView* av);
View* alert_view_get_view(AlertView* av);

void alert_view_set_record(AlertView* av, const TrackerRecord* rec);
void alert_view_tick(AlertView* av); // drives the strobe animation
void alert_view_set_ok_callback(AlertView* av, AlertViewCallback cb, void* context);
