#pragma once

#include <gui/view.h>
#include "helpers/tracker_db.h"

typedef struct DeviceListView DeviceListView;
typedef void (*DeviceListViewCallback)(void* context);

DeviceListView* device_list_view_alloc(void);
void device_list_view_free(DeviceListView* dlv);
View* device_list_view_get_view(DeviceListView* dlv);

/**
 * Replace the displayed detection set.
 *
 * The snapshot is re-sorted on every refresh (followers first, then strongest
 * signal), so a row's INDEX is not stable: a tracker whose RSSI wobbles one dB
 * swaps places with its neighbour and the highlight jumps to a different
 * device under the user's finger - and OK then opens whatever landed there.
 * Selection is therefore anchored to the device ADDRESS and the index is
 * recomputed here on every refresh.
 */
void device_list_view_set_records(DeviceListView* dlv, const TrackerRecord* recs, size_t count);

/** Copy the currently highlighted record. @return false if the list is empty. */
bool device_list_view_get_selected(DeviceListView* dlv, TrackerRecord* out);

/** Called on OK press (open detail). */
void device_list_view_set_ok_callback(
    DeviceListView* dlv,
    DeviceListViewCallback cb,
    void* context);

/**
 * What the list should say about itself.
 *
 * The empty screen used to read "Run a hunt to scan" even when a hunt WAS
 * running - telling the user to do the thing they were already doing. An empty
 * list means something different in each of these states and now says so.
 */
typedef enum {
    DeviceListStateIdle, /* no session running */
    DeviceListStateLive, /* board attached and talking */
    DeviceListStateWaiting, /* hunting, but no board has answered */
    DeviceListStateDemo, /* simulated source */
} DeviceListState;

void device_list_view_set_state(DeviceListView* dlv, DeviceListState state);
