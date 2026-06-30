/* 
 * This file is part of the INA Meter application for Flipper Zero (https://github.com/cepetr/flipper-tina).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gui/view.h>

#include "sensor/sensor_driver.h"

// Current gauge screen showing the sensor state
typedef struct CurrentGauge CurrentGauge;

typedef enum {
    CurrentGaugeButton_Menu,
    CurrentGaugeButton_DataLog,
    CurrentGaugeButton_ShuntSwitch,
} CurrentGaugeButton;

// Callback invoked when the menu button is pressed
typedef void (*CurrentGaugeButtonCallback)(void* context, CurrentGaugeButton button);

// Allocates gauge screen
CurrentGauge* current_gauge_alloc(void);

// Frees gauge screen
void current_gauge_free(CurrentGauge* gauge);

// Returns the view of the gauge screen
View* current_gauge_get_view(CurrentGauge* gauge);

// Updates the gauge screen with the sensor state
void current_gauge_update(CurrentGauge* gauge, const SensorState* state);

// Sets the callback invoked when the menu button is pressed
void current_gauge_set_button_callback(
    CurrentGauge* gauge,
    CurrentGaugeButtonCallback callback,
    void* context);
