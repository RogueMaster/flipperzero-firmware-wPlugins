#include "calculations.h"
#include <math.h>

// Physical constants definitions
const double MU_SUN = 1.32712440018e20;
const double AU_TO_KM = 149597870.7;
const double SECONDS_PER_DAY = 86400.0;
const double PI_CONST = 3.14159265358979323846;
const double TWO_PI = 6.28318530717958647692;
const double PI_OVER_180 = 0.01745329251994329577;
const double INV_PI_OVER_180 = 57.2957795130823208768;
const double ZERO = 0.0;
const double ONE = 1.0;
const double TWO = 2.0;
const double HALF = 0.5;
const double FIFTEEN = 15.0;
const double THREE_SIXTY = 360.0;
const double THOUSAND = 1000.0;

// Calculate planetary position at given day
double calculate_orbital_position(const CelestialBody* body, int day_of_year) {
    // Mean anomaly based on time since epoch
    double mean_anomaly = (TWO_PI * (double)day_of_year) / body->period_days;

    // Add starting position offset
    double start_radians = body->start_angle_deg * PI_OVER_180;
    double position = mean_anomaly + start_radians;

    // Keep angle in 0-2π range
    while(position > TWO_PI)
        position -= TWO_PI;
    while(position < ZERO)
        position += TWO_PI;

    return position;
}

// Calculate Hohmann transfer delta-v
double calculate_hohmann_delta_v(double r1_au, double r2_au) {
    // Convert AU to meters
    double r1 = r1_au * AU_TO_KM * THOUSAND;
    double r2 = r2_au * AU_TO_KM * THOUSAND;

    // First burn: enter transfer orbit
    double v1 = sqrt(MU_SUN / r1);
    double vt1 = sqrt(MU_SUN / r1) * sqrt(TWO * r2 / (r1 + r2));
    double delta_v1 = fabs(vt1 - v1);

    // Second burn: circularize at destination
    double v2 = sqrt(MU_SUN / r2);
    double vt2 = sqrt(MU_SUN / r2) * sqrt(TWO * r1 / (r1 + r2));
    double delta_v2 = fabs(v2 - vt2);

    // Convert to km/s and return total
    return (delta_v1 + delta_v2) / THOUSAND;
}

// Calculate transfer time (half period of elliptical transfer orbit)
double calculate_transfer_time(double r1_au, double r2_au) {
    double r1 = r1_au * AU_TO_KM * THOUSAND;
    double r2 = r2_au * AU_TO_KM * THOUSAND;
    double a_transfer = (r1 + r2) / TWO;
    double time_seconds = PI_CONST * sqrt(pow(a_transfer, 3.0) / MU_SUN);
    return time_seconds / SECONDS_PER_DAY; // Convert to days
}

// Calculate transfer window quality based on planetary positions
TransferResult calculate_transfer(
    const CelestialBody* destination,
    int day_of_year,
    const CelestialBody* earth) {
    TransferResult result = {0};

    // Get planetary positions
    double earth_angle = calculate_orbital_position(earth, day_of_year);
    double dest_angle = calculate_orbital_position(destination, day_of_year);

    // Calculate phase angle (relative position)
    double phase_angle = dest_angle - earth_angle;
    while(phase_angle > PI_CONST)
        phase_angle -= TWO_PI;
    while(phase_angle < -PI_CONST)
        phase_angle += TWO_PI;

    // Calculate basic Hohmann transfer
    result.delta_v_total =
        calculate_hohmann_delta_v(earth->semi_major_au, destination->semi_major_au);
    result.flight_time_days =
        calculate_transfer_time(earth->semi_major_au, destination->semi_major_au);

    // Phase angle penalty - transfers work best at specific alignments
    double ideal_phase = 0.0; // Simplified - real calculation is more complex
    double phase_penalty = fabs(phase_angle - ideal_phase);

    // Apply phase penalty to delta-v (poor alignment = higher energy needed)
    result.delta_v_total *= (ONE + phase_penalty * HALF);

    // Calculate departure angle (simplified)
    result.departure_angle = earth_angle * INV_PI_OVER_180;
    while(result.departure_angle < ZERO)
        result.departure_angle += THREE_SIXTY;
    while(result.departure_angle >= THREE_SIXTY)
        result.departure_angle -= THREE_SIXTY;

    // Determine if transfer is practical
    result.is_possible = (result.delta_v_total < FIFTEEN); // Arbitrary cutoff for "practical"

    return result;
}
