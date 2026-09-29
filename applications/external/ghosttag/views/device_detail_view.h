#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include "helpers/tracker_db.h"

typedef struct DeviceDetailView DeviceDetailView;
typedef void (*DeviceDetailViewCallback)(void* context);

DeviceDetailView* device_detail_view_alloc(void);
void device_detail_view_free(DeviceDetailView* ddv);
View* device_detail_view_get_view(DeviceDetailView* ddv);

void device_detail_view_set_record(DeviceDetailView* ddv, const TrackerRecord* rec);

/** Stamp the screen DEMO while the simulated source is running. */
void device_detail_view_set_demo(DeviceDetailView* ddv, bool demo);

/** Show "3/12" in the header so the user knows Left/Right go somewhere. */
void device_detail_view_set_position(DeviceDetailView* ddv, size_t index, size_t total);

/** Left/Right step through the detection list without returning to it. */
void device_detail_view_set_step_callbacks(
    DeviceDetailView* ddv,
    DeviceDetailViewCallback prev,
    DeviceDetailViewCallback next,
    void* context);
