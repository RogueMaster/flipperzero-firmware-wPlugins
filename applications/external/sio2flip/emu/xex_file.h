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
    // Offset in the XEX file
    uintptr_t offset;
    // Destination address
    uint16_t addr;
    // Size in bytes
    uint16_t size;
} XexBlock;

typedef struct XexFile XexFile;

// Opens XEX file
// Returns NULL if the file is not a valid XEX file
XexFile* xex_file_open(const char* name, Storage* storage);

// Closes XEX file
void xex_file_close(XexFile* xex);

// Gets XEX file size in bytes
size_t xex_file_size(XexFile* xex);

// Gets XEX file name
const char* xex_file_name(XexFile* xex);

// Gets number of blocks in the XEX file
uint16_t xex_file_block_count(XexFile* xex);

// Retrives n-th block
//
// Returns false if the block index is out of range
bool xex_file_get_block(XexFile* xex, uint16_t block_idx, XexBlock* block);

// Reads data of the n-th block to the provided buffer
//
// - 'block' is the block number (0..n-1)
// - `offset` is the offset in bytes from the start of the block data
// - `buffer` is the destination buffer
// - `size` is the size in bytes to copy
//
// Returns the number of bytes copied.
size_t xex_file_read(
    XexFile* xex,
    uint16_t block_dx,
    uintptr_t offset,
    void* buffer,
    size_t buffer_size);

// Checks if the XEX file overlaps with the given address range
bool xex_file_overlaps_with(XexFile* xex, uint16_t addr, size_t size);
