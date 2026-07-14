#pragma once

#include <gui/view.h>

typedef struct SensorView SensorView;

typedef void (*SensorViewMenuCallback)(void* context);

SensorView* sensor_view_alloc(void);
void sensor_view_free(SensorView* sensor_view);
View* sensor_view_get_view(SensorView* sensor_view);

void sensor_view_set_menu_callback(
    SensorView* sensor_view,
    SensorViewMenuCallback callback,
    void* context);
void sensor_view_update(
    SensorView* sensor_view,
    uint16_t raw_adc,
    uint16_t millivolts,
    uint8_t channel,
    uint8_t moisture_percent,
    bool connected);
