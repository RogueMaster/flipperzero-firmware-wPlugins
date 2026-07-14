#pragma once

#include <gui/view.h>

typedef struct NoSensors NoSensors;

NoSensors* no_sensors_alloc(void* context);

void no_sensors_free(NoSensors* no_sensors);

View* no_sensors_get_view(NoSensors* no_sensors);
