#pragma once

#include <gui/view.h>

typedef struct SensorInfo SensorInfo;

SensorInfo* sensor_info_alloc(void* context);

void sensor_info_free(SensorInfo* sensor_info);

View* sensor_info_get_view(SensorInfo* sensor_info);
