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
#ifndef UNITEMP_DRAW_H_
#define UNITEMP_DRAW_H_
#include "../unitemp.h"

#include <gui/elements.h>

/**
 * @brief Draws temperature reading on canvas
 * 
 * Renders the temperature value from a sensor onto the specified canvas at the given coordinates.
 * The temperature is formatted according to the specified measurement unit.
 * 
 * @param canvas Pointer to the Canvas object where the temperature will be drawn
 * @param sensor Pointer to the Sensor object containing temperature reading data
 * @param temperature_unit The unit in which temperature should be displayed (Celsius, Fahrenheit, etc.)
 * @param x Horizontal coordinate (column) on canvas where drawing starts
 * @param y Vertical coordinate (row) on canvas where drawing starts
 * 
 * @return void
 */
void unitemp_draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);
/**
 * @brief Draws a single sensor reading on the canvas
 * 
 * Renders a sensor's current temperature and status information at the specified
 * position on the canvas display. The temperature is formatted according to the
 * specified measurement unit.
 * 
 * @param canvas Pointer to the Canvas object where the sensor data will be drawn
 * @param sensor Pointer to the Sensor object containing the sensor data to display
 * @param temperature_unit The temperature unit to use for displaying the reading (Celsius, Fahrenheit, etc.)
 * @param x The x-coordinate (horizontal position) where to draw the sensor information
 * @param y The y-coordinate (vertical position) where to draw the sensor information
 * 
 * @return void
 */
void unitemp_draw_sensor_single(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);

/**
 * @brief Draws humidity information on the canvas for a given sensor
 * 
 * Renders the humidity measurement value and related information on the canvas
 * at the specified coordinates. The humidity value is displayed according to the
 * specified measurement unit.
 * 
 * @param canvas Pointer to the Canvas object for drawing operations
 * @param sensor Pointer to the Sensor object containing humidity data
 * @param hum_unit The humidity measurement unit for display formatting
 * @param temperature_unit The temperature measurement unit (may be used for context)
 * @param x Horizontal coordinate (in pixels) where drawing starts
 * @param y Vertical coordinate (in pixels) where drawing starts
 * 
 * @return void
 */
void unitemp_draw_humidity(
    Canvas* canvas,
    Sensor* sensor,
    HumidityMeausureUnit hum_unit,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);

/**
 * @brief Draws pressure value on canvas
 * 
 * Renders the pressure measurement from a sensor onto the specified canvas at the given coordinates.
 * The display format can be adjusted based on the pressure unit and display mode (mini or full).
 * 
 * @param canvas Pointer to the Canvas object where the pressure will be drawn
 * @param sensor Pointer to the Sensor object containing the pressure measurement data
 * @param pressure_unit The unit in which the pressure should be displayed (e.g., Pa, hPa, mmHg, etc.)
 * @param x The x-coordinate (column) on the canvas where drawing should start
 * @param y The y-coordinate (row) on the canvas where drawing should start
 * @param mini Flag indicating the display mode:
 *             - true: compact/mini display format
 *             - false: full display format with more details
 * 
 * @return void
 */
void unitemp_draw_pressure(
    Canvas* canvas,
    Sensor* sensor,
    PressureMeasureUnit pressure_unit,
    uint8_t x,
    uint8_t y,
    bool mini);

/**
 * @brief Draws heat index value on canvas
 *
 * Renders the heat index measurement for a given sensor on the canvas at the specified coordinates.
 * The heat index is displayed according to the specified temperature measurement unit.
 *
 * @param canvas Pointer to the Canvas object where the heat index will be drawn
 * @param sensor Pointer to the Sensor object containing the heat index data
 * @param temperature_unit The temperature unit to use for displaying the heat index (Celsius, Fahrenheit, etc.)
 * @param x The X coordinate on the canvas where drawing should begin
 * @param y The Y coordinate on the canvas where drawing should begin
 */
void unitemp_draw_heat_index(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y);

void unitemp_draw_co2(Canvas* canvas, Sensor* sensor, uint8_t x, uint8_t y, Color color, bool mini);
#endif //UNITEMP_DRAW_H_
