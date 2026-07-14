#pragma once

#include <gui/view.h>

typedef struct {
    View* view;
    void* context;
} SingleSensor;

typedef struct {
    uint8_t sensor_index;
    void* context;
} SingleSensorViewModel;

SingleSensor* single_sensor_alloc(void* context);

void single_sensor_free(SingleSensor* single_sensor);

View* single_sensor_get_view(SingleSensor* single_sensor);

void single_sensor_refresh_data(SingleSensor* instance);
