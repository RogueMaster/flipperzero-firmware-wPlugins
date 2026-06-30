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

#include <furi.h>
#include <storage/storage.h>

#include "sensor/sensor_driver.h"

typedef struct Datalog Datalog;

// Opens a new datalog file
Datalog* datalog_open(Storage* storage, const char* file_name);

// Closes the datalog file
void datalog_close(Datalog* log);

// Gets recording time in seconds
uint32_t datalog_get_duration(const Datalog* log);

// Returns number of stored records
uint32_t datalog_get_record_count(const Datalog* log);

// Returns the size of the datalog file in bytes
size_t datalog_get_file_size(Datalog* log);

// Returns the name of the datalog file.
// Callee must not free the returned string.
const FuriString* datalog_get_file_name(Datalog* log);

// Append a new record to the datalog
void datalog_append_record(Datalog* log, const SensorState* state);

// Returns a unique file name for the datalog
FuriString* datalog_build_unique_file_name(void);
