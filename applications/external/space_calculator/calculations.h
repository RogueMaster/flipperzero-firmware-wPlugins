#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <math.h>
#include <stdbool.h>

// Physical constants - all explicitly double to avoid promotion warnings
extern const double MU_SUN;
extern const double AU_TO_KM;
extern const double SECONDS_PER_DAY;
extern const double PI_CONST;
extern const double TWO_PI;
extern const double PI_OVER_180;
extern const double INV_PI_OVER_180;
extern const double ZERO;
extern const double ONE;
extern const double TWO;
extern const double HALF;
extern const double FIFTEEN;
extern const double THREE_SIXTY;

// Celestial body structure
typedef struct {
    const char* name;
    double period_days;
    double semi_major_au;
    double eccentricity;
    double start_angle_deg;
    int parent_body;
    bool is_moon;
} CelestialBody;

// Transfer result structure
typedef struct {
    double delta_v_total;
    double flight_time_days;
    double departure_angle;
    bool is_possible;
} TransferResult;

// Core calculation functions
double calculate_orbital_position(const CelestialBody* body, int day_of_year);
double calculate_hohmann_delta_v(double r1_au, double r2_au);
double calculate_transfer_time(double r1_au, double r2_au);
TransferResult calculate_transfer(
    const CelestialBody* destination,
    int day_of_year,
    const CelestialBody* earth);

#endif // CALCULATIONS_H
