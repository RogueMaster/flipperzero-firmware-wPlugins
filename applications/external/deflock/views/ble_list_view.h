// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <gui/view.h>

// Custom "Guardian HUD" list view for the BLE / Tracker scan screen. Mirrors
// flock_view: inverted title bar, a status sub-line, selectable action rows
// (e.g. "Save Report") followed by per-device rows with signal-strength bars.
// All device data is read live from the owning ReconApp inside the draw.
typedef struct BleListView BleListView;

/** OK-press callback. selected_index is the raw row index (action rows first,
 *  then device rows); the scene maps it to an action or a device. */
typedef void (*BleListViewOkCallback)(void* context, int selected_index);

BleListView* ble_list_view_alloc(void);
void ble_list_view_free(BleListView* v);
View* ble_list_view_get_view(BleListView* v);

/** Set the owning ReconApp pointer (read for live data inside the draw). */
void ble_list_view_set_app(BleListView* v, void* app);

/** Set the OK-press callback. */
void ble_list_view_set_ok_callback(BleListView* v, BleListViewOkCallback cb, void* context);

/** Title-bar right value (copied). NULL clears it. */
void ble_list_view_set_right(BleListView* v, const char* right);

/** Full status sub-line under the title bar (copied). NULL clears it. */
void ble_list_view_set_header(BleListView* v, const char* header);

/** Centered status message shown when there are no device rows (copied). NULL = none. */
void ble_list_view_set_status(BleListView* v, const char* status);

/** Action rows shown above the device list. labels must outlive the view
 *  (string literals). count 0 = no action rows. */
void ble_list_view_set_actions(BleListView* v, const char* const* labels, int count);

/** Request a redraw (call from a GUI-thread tick; never hold app->mutex). */
void ble_list_view_refresh(BleListView* v);

/** Reset selection/scroll to the top. */
void ble_list_view_reset(BleListView* v);
