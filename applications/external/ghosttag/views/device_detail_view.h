#pragma once

#include <gui/view.h>
#include "helpers/tracker_db.h"

typedef struct DeviceDetailView DeviceDetailView;

DeviceDetailView* device_detail_view_alloc(void);
void device_detail_view_free(DeviceDetailView* ddv);
View* device_detail_view_get_view(DeviceDetailView* ddv);

void device_detail_view_set_record(DeviceDetailView* ddv, const TrackerRecord* rec);
