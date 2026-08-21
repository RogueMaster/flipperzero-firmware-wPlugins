// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

#include <gui/view.h>

// Pwnagotchi-style "Net Guardian" display: a calm idle face that shows the
// fused WATCHSCORE state (CLEAR / WATCHFUL / ELEVATED), the per-signal
// breakdown, the live sweep mode + radio counters, and an uptime clock. All
// data is read live from the owning ReconApp inside the draw callback.
typedef struct GuardianView GuardianView;

GuardianView* guardian_view_alloc(void);
void guardian_view_free(GuardianView* gv);
View* guardian_view_get_view(GuardianView* gv);

/** Set the owning ReconApp pointer (read for live data inside the draw). */
void guardian_view_set_app(GuardianView* gv, void* app);

/** Called on a short OK press (used to open the Suspicious-devices list). */
void guardian_view_set_ok_callback(GuardianView* gv, void (*cb)(void*), void* ctx);

/** RIGHT opens the guarded-network picker. Separate from OK so the Suspicious
 *  list keeps the key it has always had -- retraining muscle memory to add a
 *  feature is a poor trade on a device with five buttons. */
void guardian_view_set_right_callback(GuardianView* gv, void (*cb)(void*), void* ctx);

/** LEFT opens the attack-detail screen (triage + evidence + advice). Separate
 *  key from OK/Right so each drill-down keeps a stable button. */
void guardian_view_set_left_callback(GuardianView* gv, void (*cb)(void*), void* ctx);

/** Request a redraw (call from a GUI-thread tick; never hold app->mutex). */
void guardian_view_refresh(GuardianView* gv);
