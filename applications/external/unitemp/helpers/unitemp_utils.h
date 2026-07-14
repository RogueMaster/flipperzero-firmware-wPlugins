/*
    Unitemp - Universal temperature reader
    Copyright (C) 2022-2026  Victor Nikitchuk (https://github.com/quen0n)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef UNTIEMP_UTILS_H_
#define UNTIEMP_UTILS_H_

#include "notification/notification.h"
#include "../sensors.h"

typedef enum {
    EnvironmentStateUndefined,
    EnvironmentStateExcellent,
    EnvironmentStateGood,
    EnvironmentStateModerate,
    EnvironmentStatePoor,
    EnvironmentStateDangerous
} EnvironmentState;

/**
 * @brief Calculates the dew point temperature based on ambient temperature and humidity.
 *
* Computes the dew point temperature using the provided ambient temperature
 * and relative humidity values. The dew point is the temperature at which
 * water vapor in the air condenses into liquid water.
 * 
 * @param temperature_in_celsius The ambient air temperature in degrees Celsius.
 * @param humidity_in_percent The relative humidity in percent (0-100).
 * 
 * @return The calculated dew point temperature in degrees Celsius.
 * 
 * @note This function uses an empirical approximation and is suitable for
 *       temperatures in the range of -40°C to +50°C and humidity from 1% to 100%.
 *
 * @see https://en.wikipedia.org/wiki/Dew_point
 */
float unitemp_calculate_dew_point(float temperature_in_celsius, float humidity_in_percent);

/**
 * @brief Calculates the heat index based on temperature and humidity.
 * 
 * The heat index is a measure of how hot it actually feels when relative humidity
 * is factored in with the actual air temperature. This function uses the standard
 * heat index formula to compute the apparent temperature.
 * 
 * @param temperature_in_fahrenheit The ambient air temperature in degrees Fahrenheit.
 * @param humidity_in_percent The relative humidity as a percentage (0-100).
 * 
 * @return The calculated heat index value in degrees Fahrenheit.
 * 
 * @note The heat index is typically defined for temperatures above 70°F (21°C).
 *       For lower temperatures, the function may still return a value but it should
 *       not be relied upon for accuracy.
 * 
 * @see https://en.wikipedia.org/wiki/Heat_index for more information about heat index calculation.
 */
float unitemp_calculate_heat_index(float temperature_in_fahrenheit, float humidity_in_percent);

/**
 * @brief Convert pressure from Pascals to millimeters of mercury
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in mmHg
 */
float unitemp_convert_pa_to_mm_hg(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to inches of mercury
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in inHg
 */
float unitemp_convert_pa_to_in_hg(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to kilopascals
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in kPa
 */
float unitemp_convert_pa_to_kpa(float pressure_in_pa);

/**
 * @brief Convert pressure from Pascals to hectopascals
 * @param pressure_in_pa Pressure value in Pascals
 * @return Pressure value in hPa
 */
float unitemp_convert_pa_to_hpa(float pressure_in_pa);

EnvironmentState unitemp_determine_environment_state_from_hi(float heat_index_in_fahrenheit);

EnvironmentState unitemp_determine_environment_state_from_co2(uint16_t ppm);

EnvironmentState unitemp_determine_environment_state(Sensor* sensor);

void unitemp_display_environment_state(
    NotificationApp* app,
    EnvironmentState state,
    bool light,
    bool vibro_and_sound);

void unitemp_reset_environment_state(NotificationApp* app);
#endif //#UNTIEMP_UTILS_H_
