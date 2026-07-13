#pragma once

#include <gui/view.h>
#include "../helpers/rf_scope.h"

/* Draws the live Sub-GHz RF envelope: a scrolling scope of RSSI over time with
 * a "carrier present" threshold line. When the trace pokes above the line, that
 * is an OOK burst - the modulation from the walkthrough, happening for real. */

typedef struct ScopeView ScopeView;

ScopeView* scope_view_alloc(void);
void scope_view_free(ScopeView* v);
View* scope_view_get_view(ScopeView* v);

void scope_view_reset(ScopeView* v);
void scope_view_set_snapshot(ScopeView* v, const RfSnapshot* snap);
