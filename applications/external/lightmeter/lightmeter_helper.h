#pragma once

#include <math.h>
#include <stdint.h>

#include "lightmeter_config.h"

float lux2ev(float lux, LightMeterISONumbers iso);

float getMinDistance(float x, float v1, float v2);

float normalizeAperture(float a);

float normalizeTime(float a);
