// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/view.h>

// Locator "homing" HUD: hunt one marked device down by live signal strength.
// Shows a hot/cold RSSI meter, the dBm reading, a peak-hold marker, a
// warmer/colder trend, and the signal's age (so a quiet/out-of-range target
// reads as such). Works with no GPS; if a fix is present it notes where the
// signal peaked. All data is read live from the owning ReconApp in the draw.
typedef struct LocatorView LocatorView;

LocatorView* locator_view_alloc(void);
void locator_view_free(LocatorView* lv);
View* locator_view_get_view(LocatorView* lv);

/** Set the owning ReconApp pointer (read for live data inside the draw). */
void locator_view_set_app(LocatorView* lv, void* app);

/** Clear peak/trend history for a fresh hunt (call from the scene on_enter). */
void locator_view_reset(LocatorView* lv);

/** Request a redraw (call from a GUI-thread tick; never hold app->mutex). */
void locator_view_refresh(LocatorView* lv);
