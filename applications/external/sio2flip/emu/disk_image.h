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

typedef struct {
    // Nubner of heads (1==Single sided, 2==Double sided)
    uint8_t heads;
    // Number of tracks per head
    uint8_t tracks;
    // Number of sectors per track
    uint16_t sectors_per_track;
    // Sector size in bytes
    uint16_t sector_size;
    // Density (0 for FM/Single, 4 for MFM/Double)
    uint8_t density;
} DiskGeometry;

typedef struct DiskImage DiskImage;

// Opens existing ATR disk image file
DiskImage* disk_image_open(const char* path, Storage* storage);

// Creates a new empty ATR disk image file
DiskImage* disk_image_create(const char* path, Storage* storage);

// Closes the disk image
void disk_image_close(DiskImage* image);

// Returns the path to the disk image.
const FuriString* disk_image_path(const DiskImage* image);

DiskGeometry disk_geometry(const DiskImage* image);

// Returns the size of the image in bytes
size_t disk_image_size(const DiskImage* image);

// Returns the sector size in bytes
size_t disk_image_sector_size(const DiskImage* image);

// Returns the number of sectors in the disk image
size_t disk_image_sector_count(const DiskImage* image);

// Returns the write protect status of the disk image
bool disk_image_get_write_protect(const DiskImage* image);

// Sets the write protect status of the disk image
bool disk_image_set_write_protect(DiskImage* image, bool write_protect);

// Returns the size of the specified sector in bytes
//
// Returns 0 if the sector is out of range
size_t disk_image_nth_sector_size(const DiskImage* image, size_t sector);

// Reads a sector from the disk image
//  - `sector` is 1-based
// - `buffer` must be at least `disk_image_sector_size(image)` bytes long
bool disk_image_read_sector(DiskImage* image, size_t sector, void* buffer);

// Writes a sector to the disk image
// - `sector` is 1-based
// - `buffer` must be at least `disk_image_sector_size(image)` bytes long
bool disk_image_write_sector(DiskImage* image, size_t sector, const void* buffer);

// Formats the disk image with the specified geometry.
// The disk image is resized and all sectors are cleared.
bool disk_image_format(DiskImage* image, DiskGeometry geometry);

// Determines the disk geometry based on the disk size and sector size
// Returns false if the geometry cannot be determined.
bool determine_disk_geometry(DiskGeometry* geom, size_t disk_size, size_t sector_size);
