// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/view.h>

// Custom "Guardian HUD" list view for the WiFi Audit screen. Mirrors
// flock_view: inverted title bar, a status sub-line, selectable action rows
// ("Rescan" / "Save Report") followed by per-AP rows with a security grade,
// evil-twin markers, and signal-strength bars. AP data is read live from the
// owning ReconApp inside the draw.
typedef struct WifiListView WifiListView;

/** OK-press callback. selected_index is the raw row index (action rows first,
 *  then AP rows); the scene maps it to an action or an AP. */
typedef void (*WifiListViewOkCallback)(void* context, int selected_index);

WifiListView* wifi_list_view_alloc(void);
void wifi_list_view_free(WifiListView* v);
View* wifi_list_view_get_view(WifiListView* v);

/** Set the owning ReconApp pointer (read for live data inside the draw). */
void wifi_list_view_set_app(WifiListView* v, void* app);

/** Set the OK-press callback. */
void wifi_list_view_set_ok_callback(WifiListView* v, WifiListViewOkCallback cb, void* context);

/** Title-bar right value (copied). NULL clears it. */
void wifi_list_view_set_right(WifiListView* v, const char* right);

/** Full status sub-line under the title bar (copied). NULL clears it. */
void wifi_list_view_set_header(WifiListView* v, const char* header);

/** Centered status message shown when there are no AP rows (copied). NULL = none. */
void wifi_list_view_set_status(WifiListView* v, const char* status);

/** Action rows shown above the AP list. labels must outlive the view
 *  (string literals). count 0 = no action rows. */
void wifi_list_view_set_actions(WifiListView* v, const char* const* labels, int count);

/** Request a redraw (call from a GUI-thread tick; never hold app->mutex). */
void wifi_list_view_refresh(WifiListView* v);

/** Reset selection/scroll to the top. */
void wifi_list_view_reset(WifiListView* v);
