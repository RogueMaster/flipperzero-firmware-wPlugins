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
#include "datalog.h"

// DatalogScreen screen showing the sensor state
typedef struct DatalogScreen DatalogScreen;

typedef enum {
    DatalogScreenButton_StartStop,
} DatalogScreenButton;

// Callback invoked when the menu button is pressed
typedef void (*DatalogScreenButtonCallback)(void* context, DatalogScreenButton button);

// Allocates datalog screen
DatalogScreen* datalog_screen_alloc(void);

// Frees datalog screen
void datalog_screen_free(DatalogScreen* screen);

// Returns the view of the datalog screen
View* datalog_screen_get_view(DatalogScreen* screen);

// Updates the datalog screen with the sensor state
void datalog_screen_update(DatalogScreen* screen, Datalog* log);

// Sets the callback invoked when the menu button is pressed
void datalog_screen_set_button_callback(
    DatalogScreen* screen,
    DatalogScreenButtonCallback callback,
    void* context);
