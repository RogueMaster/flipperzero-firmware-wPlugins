#pragma once
#include <gui/view.h>
#include "../link.h"

typedef struct PhrDashboardView PhrDashboardView;
typedef void (*PhrDashboardCallback)(void* context);

PhrDashboardView* phr_dashboard_view_alloc(void);
void phr_dashboard_view_free(PhrDashboardView* v);
View* phr_dashboard_view_get_view(PhrDashboardView* v);
/** Invoked when the user presses OK (open settings). */
void phr_dashboard_view_set_ok_callback(PhrDashboardView* v, PhrDashboardCallback cb, void* ctx);
void phr_dashboard_view_update(
    PhrDashboardView* v,
    const PhrLinkSnapshot* snap,
    const char* transport,
    const char* device,
    const char* status);
