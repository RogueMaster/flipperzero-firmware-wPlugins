#ifndef DILATE_CALCULATIONS_H
#define DILATE_CALCULATIONS_H

#include <math.h>
#include <stdbool.h>

// Time units for cycling
typedef enum {
    UNIT_HOURS,
    UNIT_DAYS,
    UNIT_YEARS,
    UNIT_COUNT
} TimeUnit;

// Constants
extern const double HOURS_TO_SECONDS;
extern const double DAYS_TO_SECONDS;
extern const double YEARS_TO_SECONDS;
extern const double ZERO;
extern const double ONE;

// Core calculation functions
double lorentz_factor(double v_over_c);
double time_to_seconds(double value, TimeUnit unit);

#endif // DILATE_CALCULATIONS_H
