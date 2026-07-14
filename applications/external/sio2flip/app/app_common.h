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

#include <furi.h>

#define TAG "sio2flip"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// Number of emulated FDD peripherals
#define FDD_EMULATOR_COUNT 4

void set_last_error(const char* title, const char* fmt, ...);

FuriString* get_last_error_title(void);

FuriString* get_last_error_message(void);
