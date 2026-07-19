#pragma once

#include <gui/view.h>
#include "helpers/tracker_db.h"

typedef struct DeviceListView DeviceListView;
typedef void (*DeviceListViewCallback)(void* context);

DeviceListView* device_list_view_alloc(void);
void device_list_view_free(DeviceListView* dlv);
View* device_list_view_get_view(DeviceListView* dlv);

/** Replace the displayed detection set (keeps selection in range). */
void device_list_view_set_records(DeviceListView* dlv, const TrackerRecord* recs, size_t count);

/** Copy the currently highlighted record. @return false if the list is empty. */
bool device_list_view_get_selected(DeviceListView* dlv, TrackerRecord* out);

/** Called on OK press (open detail). */
void device_list_view_set_ok_callback(DeviceListView* dlv, DeviceListViewCallback cb, void* context);
