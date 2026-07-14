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

#include "unitemp_draw.h"
#include "unitemp_icons.h"

#include <stdlib.h>
#include <inttypes.h>

#include <gui/elements.h>
#include <locale/locale.h>
#include "../helpers/unitemp_utils.h"

#define TEMP_STR_SIZE 32
static char temp_str[TEMP_STR_SIZE];

void unitemp_draw_temperature(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    //Не рисовать, если координаты равны UT_DATA_POS_NONE
    if(x == 255 && y == 255) return;
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    float temperature = sensor->temperature;
    if(temperature_unit == UT_TEMP_FAHRENHEIT) {
        temperature = locale_celsius_to_fahrenheit(temperature);
    }
    int8_t temp_dec = abs((int16_t)(temperature * 10) % 10);

    //Drawing icon
    canvas_draw_icon(
        canvas,
        x + 3,
        y + 3,
        (temperature_unit == UT_TEMP_CELSIUS ? &I_temp_C_11x14 : &I_temp_F_11x14));

    if(!((sensor->status == UT_SENSORSTATUS_OK && sensor->temperature != -128.0f) ||
         (sensor->status == UT_SENSORSTATUS_POLLING && sensor->temperature != -128.0f))) {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        return;
    }

    //Whole part of temperature
    //A crutch for displaying the sign of a number less than 0
    uint8_t offset = 0;
    if(temperature < 0 && temperature > -1) {
        temp_str[0] = '-';
        offset = 1;
    }
    snprintf((char*)(temp_str + offset), TEMP_STR_SIZE, "%d", (int16_t)temperature);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((temperature <= -10 || temperature > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the temperature in the range from -9 to 99 (when there are two digits in the number)
    if(temperature > -10 && temperature <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", temp_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
}

void unitemp_draw_sensor_single(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    canvas_set_font(canvas, FontPrimary);

    const uint8_t max_width = 56;

    char sensor_name[11] = {0};
    memcpy(sensor_name, sensor->name, 10);

    if(canvas_string_width(canvas, sensor_name) > max_width) {
        uint8_t i = 10;
        while((canvas_string_width(canvas, sensor_name) > max_width - 6) && (i != 0)) {
            sensor_name[i--] = '\0';
        }
        sensor_name[++i] = '.';
        sensor_name[++i] = '.';
    }

    canvas_draw_str_aligned(canvas, x + 27, y + 3, AlignCenter, AlignCenter, sensor_name);
    unitemp_draw_temperature(canvas, sensor, temperature_unit, x, y + 8);
}

void unitemp_draw_humidity(
    Canvas* canvas,
    Sensor* sensor,
    HumidityMeausureUnit hum_unit,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    //Не рисовать, если координаты равны UT_DATA_POS_NONE
    if(x == 255 && y == 255) return;
    // Drawing the frame
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    if(hum_unit == UT_HUMIDITY_RELATIVE) {
        // Drawing the icon
        canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_relative_9x15);
        // Relative humidity
        snprintf(temp_str, TEMP_STR_SIZE, "%d", (uint8_t)sensor->humidity);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, temp_str);
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        // Adding '%' for relative humidity
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 4, y + 10 + 7, "%");
    } else if(hum_unit == UT_HUMIDITY_DEW_POINT) {
        float dew_point = unitemp_calculate_dew_point(sensor->temperature, sensor->humidity);

        if(temperature_unit == UT_TEMP_CELSIUS) {
            canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_dewpoint_c_9x15);
        } else {
            canvas_draw_icon(canvas, x + 3, y + 2, &I_hum_dewpoint_f_9x15);
            dew_point = locale_celsius_to_fahrenheit(dew_point);
        }

        // Dewpoint with a decimal
        int humidity_dec = abs((int16_t)(dew_point * 10) % 10);
        snprintf(temp_str, TEMP_STR_SIZE, "%d", (int16_t)dew_point);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, temp_str);
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        // Printing the decimal part similar to temperature display
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", humidity_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
}

void unitemp_draw_pressure(
    Canvas* canvas,
    Sensor* sensor,
    PressureMeasureUnit pressure_unit,
    uint8_t x,
    uint8_t y,
    bool mini) {
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, mini ? 54 : 76, 20, 3);
    canvas_draw_rframe(canvas, x, y, mini ? 54 : 76, 19, 3);

    //Drawing icon
    canvas_draw_icon(canvas, x + 3, y + 4, &I_pressure_7x13);

    float pressure = sensor->pressure;

    if(pressure_unit == UT_PRESSURE_MM_HG) {
        pressure = unitemp_convert_pa_to_mm_hg(pressure);
    } else if(pressure_unit == UT_PRESSURE_IN_HG) {
        pressure = unitemp_convert_pa_to_in_hg(pressure);
    } else if(pressure_unit == UT_PRESSURE_KPA) {
        pressure = unitemp_convert_pa_to_kpa(pressure);
    } else if(pressure_unit == UT_PRESSURE_HPA) {
        pressure = unitemp_convert_pa_to_hpa(pressure);
    }

    int16_t press_int = pressure;
    int8_t press_dec = (int16_t)(pressure * 10) % 10;

    //Whole part of the pressure
    snprintf(temp_str, TEMP_STR_SIZE, "%d", press_int);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 28 + ((press_int > 99) ? 5 : 0) - ((press_int > 999 && mini) ? 3 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);
    //Printing the fractional part of the pressure in the range from 0 to 99 (when there are two digits in the number)
    if(press_int <= 99) {
        uint8_t int_len = canvas_string_width(canvas, temp_str);
        snprintf(temp_str, TEMP_STR_SIZE, ".%d", press_dec);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
    }
    if(!mini) {
        canvas_set_font(canvas, FontSecondary);
        //A unit of measurement
        if(pressure_unit == UT_PRESSURE_MM_HG) {
            canvas_draw_icon(canvas, x + 56, y + 2, &I_mm_hg_15x15);
        } else if(pressure_unit == UT_PRESSURE_IN_HG) {
            canvas_draw_icon(canvas, x + 56, y + 2, &I_in_hg_15x15);
        } else if(pressure_unit == UT_PRESSURE_KPA) {
            canvas_draw_str(canvas, x + 57, y + 13, "kPa");
        } else if(pressure_unit == UT_PRESSURE_HPA) {
            canvas_draw_str(canvas, x + 58, y + 13, "hPa");
        }
    }
}

void unitemp_draw_heat_index(
    Canvas* canvas,
    Sensor* sensor,
    TempMeasureUnit temperature_unit,
    uint8_t x,
    uint8_t y) {
    canvas_draw_rframe(canvas, x, y, 54, 20, 3);
    canvas_draw_rframe(canvas, x, y, 54, 19, 3);

    canvas_draw_icon(canvas, x + 3, y + 3, &I_heat_index_11x14);

    float temperature = sensor->temperature;

    float heat_index;
    if(temperature >= 21.0f) {
        temperature = locale_celsius_to_fahrenheit(sensor->temperature);
        heat_index = unitemp_calculate_heat_index(temperature, sensor->humidity);
        if(temperature_unit == UT_TEMP_CELSIUS) {
            heat_index = locale_fahrenheit_to_celsius(heat_index);
        }
    } else {
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 27, y + 10, AlignCenter, AlignCenter, "--");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, x + 50, y + 10 + 3, AlignRight, AlignCenter, ". -");
        return;
    }

    int16_t heat_index_int = heat_index;
    int8_t heat_index_dec = abs((int16_t)(heat_index * 10) % 10);

    snprintf(temp_str, TEMP_STR_SIZE, "%d", heat_index_int);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas,
        x + 27 + ((heat_index <= -10 || heat_index > 99) ? 5 : 0),
        y + 10,
        AlignCenter,
        AlignCenter,
        temp_str);

    uint8_t int_len = canvas_string_width(canvas, temp_str);
    snprintf(temp_str, TEMP_STR_SIZE, ".%d", heat_index_dec);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x + 27 + int_len / 2 + 2, y + 10 + 7, temp_str);
}

void unitemp_draw_co2(Canvas* canvas, Sensor* sensor, uint8_t x, uint8_t y, Color color, bool mini) {
    const uint8_t frame_w = mini ? 54 : 83;
    //Drawing a frame
    canvas_draw_rframe(canvas, x, y, frame_w, 20, 3);
    if(color == ColorBlack) {
        canvas_draw_rbox(canvas, x, y, frame_w, 19, 3);
        canvas_invert_color(canvas);
    } else {
        canvas_draw_rframe(canvas, x, y, frame_w, 19, 3);
    }
    //Drawing icon
    canvas_draw_icon(canvas, x + 3, y + 3, &I_co2_11x14);

    uint32_t concentration_int = (uint32_t)sensor->co2;

    if(mini) {
        if(concentration_int > 40000u || concentration_int == 0) {
            snprintf(temp_str, TEMP_STR_SIZE, "--");
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, x + 34, y + 10, AlignCenter, AlignCenter, temp_str);
        } else if(concentration_int <= 999) {
            snprintf(temp_str, TEMP_STR_SIZE, "%ld", concentration_int);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, x + 34, y + 10, AlignCenter, AlignCenter, temp_str);
        } else if(concentration_int > 999 && concentration_int <= 9999) {
            snprintf(temp_str, TEMP_STR_SIZE, "%ld", concentration_int / 1000);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, x + 32, y + 10, AlignRight, AlignCenter, temp_str);
            uint8_t a = concentration_int % 1000 / 100;
            snprintf(temp_str, TEMP_STR_SIZE, ".%dk", a);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, x + 34, y + 10 + 7, temp_str);
        } else {
            snprintf(temp_str, TEMP_STR_SIZE, "%ld", concentration_int / 1000);
            canvas_set_font(canvas, FontBigNumbers);
            canvas_draw_str_aligned(canvas, x + 41, y + 10, AlignRight, AlignCenter, temp_str);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, x + 43, y + 17, "k");
        }
    } else {
        if(concentration_int > 40000u || concentration_int == 0) {
            snprintf(temp_str, TEMP_STR_SIZE, "--");
        } else {
            snprintf(temp_str, TEMP_STR_SIZE, "%ld", concentration_int);
        }

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, x + 49, y + 10, AlignCenter, AlignCenter, temp_str);
    }
}
