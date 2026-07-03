#include "calculations.h"
#include <math.h>

// Constants definitions
const double HOURS_TO_SECONDS = 3600.0;
const double DAYS_TO_SECONDS = 86400.0;
const double YEARS_TO_SECONDS = 31536000.0;
const double ZERO = 0.0;
const double ONE = 1.0;

// Physics calculations - verified formulas
double lorentz_factor(double v_over_c) {
    // Prevent division by zero at light speed
    if(v_over_c >= ONE) return (double)INFINITY;
    if(v_over_c < ZERO) return ONE; // Handle negative velocities
    double v_squared = v_over_c * v_over_c;
    return ONE / sqrt(ONE - v_squared);
}

// Convert time to seconds for consistent calculations
double time_to_seconds(double value, TimeUnit unit) {
    switch(unit) {
    case UNIT_HOURS:
        return value * HOURS_TO_SECONDS;
    case UNIT_DAYS:
        return value * DAYS_TO_SECONDS;
    case UNIT_YEARS:
        return value * YEARS_TO_SECONDS;
    default:
        return value;
    }
}
