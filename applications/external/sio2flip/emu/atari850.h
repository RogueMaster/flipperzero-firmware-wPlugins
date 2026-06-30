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
#include <storage/storage.h>

#include "app/app_config.h"

#include "sio_driver.h"

typedef struct Atari850 Atari850;

// Creates a Atari 850 emulator
Atari850* atari850_alloc(AppConfig* config);

// Frees the Atari 850 emulator
void atari850_free(Atari850* emu);

// Loads the ROM image from the SD card
void atari850_load_rom(Atari850* emu, Storage* storage);

// Starts the Atari 850 emulator on specified SIO driver
bool atari850_start(Atari850* emu, SIODriver* sio);

// Stops the Atari 850 emulator
void atari850_stop(Atari850* emu);

typedef void (*Atari850ActivityCallback)(void* context);

void atari850_set_activity_callback(
    Atari850* emu,
    Atari850ActivityCallback callback,
    void* context);
