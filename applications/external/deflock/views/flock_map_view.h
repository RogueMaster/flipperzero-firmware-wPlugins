// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/view.h>

typedef struct FlockMapView FlockMapView;

FlockMapView* flock_map_view_alloc(void);
void flock_map_view_free(FlockMapView* fmv);
View* flock_map_view_get_view(FlockMapView* fmv);

/** Set the owning ReconApp pointer (read for live data inside the draw callback). */
void flock_map_view_set_app(FlockMapView* fmv, void* app);

/** Request a redraw (call from a GUI-thread tick; never hold app->mutex). */
void flock_map_view_refresh(FlockMapView* fmv);
