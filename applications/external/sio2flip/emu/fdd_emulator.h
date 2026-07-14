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

#include "app/app_config.h"

#include "sio_driver.h"
#include "disk_image.h"

typedef struct FddEmulator FddEmulator;

// Creates a FDD emulator
FddEmulator* fdd_alloc(SIODevice device, AppConfig* config);

// Frees the FDD emulator
void fdd_free(FddEmulator* fdd);

// Starts the FDD emulator on specified SIO driver
bool fdd_start(FddEmulator* fdd, SIODriver* sio);

// Stops the FDD emulator
void fdd_stop(FddEmulator* fdd);

typedef enum {
    FddEmuActivity_Other,
    FddEmuActivity_Read,
    FddEmuActivity_Write,
} FddActivity;

typedef void (
    *FddActivityCallback)(void* context, SIODevice device, FddActivity activity, uint16_t sector);

void fdd_set_activity_callback(FddEmulator* fdd, FddActivityCallback callback, void* context);

// Returns the FDD device
SIODevice fdd_get_device(FddEmulator* fdd);

// Returns loaded disk image
DiskImage* fdd_get_disk(FddEmulator* fdd);

// Returns the last accessed sector
size_t fdd_get_last_sector(FddEmulator* fdd);

// Load a disk image into the FDD emulator
bool fdd_insert_disk(FddEmulator* fdd, DiskImage* image);

// Eject the disk from the FDD emulator
void fdd_eject_disk(FddEmulator* fdd);

// Swap disk images between two FDD emulators
void fdd_swap_disk(FddEmulator* fdd, FddEmulator* other_fdd);
