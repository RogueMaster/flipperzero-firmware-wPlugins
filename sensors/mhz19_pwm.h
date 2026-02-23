#pragma once

#include <stdint.h>

typedef enum { RANGE_2000 = 2000, RANGE_5000 = 5000 } SensorRange;

int32_t calculate_ppm(
    int32_t* prevVal,
    int32_t val,
    int32_t* th,
    int32_t* tl,
    int32_t* h,
    int32_t* l,
    SensorRange range);
