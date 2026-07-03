/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
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

#include <storage/storage.h>

#include "utils/slice.h"

#include "app_common.h"

typedef struct {
    // Enum value
    uint32_t value;
    // Identifier in the configuration file
    const char* id;
    // Text representation
    const char* text;
} AppConfigOption;

typedef enum {
    SpeedIndex_40_19K = 40,
    SpeedIndex_16_38K = 16,
    SpeedIndex_8_56K = 8,
    SpeedIndex_7_61K = 7,
    SpeedIndex_6_68K = 6,
    SpeedIndex_5_74K = 5,
    SpeedIndex_4_81K = 4,
    SpeedIndex_3_89K = 3,
    SpeedIndex_2_99K = 2,
    SpeedIndex_1_111K = 1,
    SpeedIndex_0_127K = 0,
} SpeedIndex;

// All possible high-speed indexes
extern const AppConfigOption speed_index_options[10];

// Finds the high-speed index option by its value
const AppConfigOption* speed_index_by_value(SpeedIndex value);

// Finds the high-speed index option by its identifier
const AppConfigOption* speed_index_by_id(Slice slice);

typedef enum {
    SpeedMode_Standard,
    SpeedMode_USDoubler,
    SpeedMode_XF551,
} SpeedMode;

// All possible speed modes
extern const AppConfigOption speed_mode_options[3];

// Finds the speed mode option by its value
const AppConfigOption* speed_mode_by_value(SpeedMode value);

// Finds the speed mode option by its identifier
const AppConfigOption* speed_mode_by_id(Slice slice);

// Application configuration
typedef struct {
    // Indicate an activity  by blinking the LED
    bool led_blinking;

    // Speed mode
    SpeedMode speed_mode;
    // Speed index (used if speed mode is not standard)
    uint8_t speed_index;

    struct {
        // Disk image file name
        // (empty string means no disk inserted)
        FuriString* image;
    } fdd[FDD_EMULATOR_COUNT];

    // Emulates Atari 850 modem
    bool atari850;

} AppConfig;

// Initializes the application configuration and sets the default values
void app_config_init(AppConfig* config);

// Frees memory allocated for the application configuration
void app_config_free(AppConfig* config);

// Saves the application configuration to the storage
void app_config_save(const AppConfig* config, Storage* storage);

// Loads the application configuration from the storage
void app_config_load(AppConfig* config, Storage* storage);
