#pragma once

#include "../co2_app.h"

typedef struct Co2App Co2App;

void co2_bme280_init(Co2App* app);
void co2_bme280_deinit(Co2App* app);
bool co2_bme280_read(Co2App* app);
