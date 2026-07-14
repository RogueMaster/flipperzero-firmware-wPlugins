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

#include "app/app_config.h"

#include "xex_file.h"

#define ERROR_TITLE "Executable not loaded"

#define MAX_XEX_BLOCKS 32

struct XexFile {
    FuriString* name;
    File* file;
    size_t file_size;
    XexBlock blocks[MAX_XEX_BLOCKS];
    uint16_t block_count;
};

static bool xex_file_parse(XexFile* xex);

XexFile* xex_file_open(const char* name, Storage* storage) {
    XexFile* xex = (XexFile*)malloc(sizeof(XexFile));
    memset(xex, 0, sizeof(XexFile));

    xex->file = storage_file_alloc(storage);

    FURI_LOG_I(TAG, "Opening XEX file %s", name);

    xex->name = furi_string_alloc_set(name);

    if(!storage_file_open(xex->file, name, FSAM_READ, FSOM_OPEN_EXISTING)) {
        set_last_error(ERROR_TITLE, "Failed to open file %s", name);
        goto cleanup;
    }

    xex->file_size = storage_file_size(xex->file);
    FURI_LOG_I(TAG, "XEX file size: %d bytes", xex->file_size);

    if(!xex_file_parse(xex)) {
        set_last_error(ERROR_TITLE, "Failed to parse file %s", name);
        goto cleanup;
    }

    return xex;

cleanup:
    xex_file_close(xex);
    return NULL;
}

bool xex_file_parse(XexFile* xex) {
    uintptr_t offset = 0;

    if(!storage_file_seek(xex->file, offset, true)) {
        return false;
    }

    xex->block_count = 0;

    while(offset < xex->file_size) {
        uint16_t header[2];

        if(storage_file_read(xex->file, &header[0], 4) != 4) {
            return false;
        }
        offset += 4;

        if(header[0] == 0xFFFF) {
            header[0] = header[1];
            if(storage_file_read(xex->file, &header[1], 2) != 2) {
                return false;
            }
            offset += 2;
        }

        if(xex->block_count == MAX_XEX_BLOCKS) {
            return false;
        }

        if(header[0] == 0x1A1A && header[1] == 0x1A1A) {
            offset -= 4;
            FURI_LOG_I(TAG, "XEX file truncated from %d to %d bytes", xex->file_size, offset);
            xex->file_size = offset;
            break;
        }

        XexBlock* block = &xex->blocks[xex->block_count];

        block->offset = offset;
        block->addr = header[0];
        block->size = header[1] - header[0] + 1;

        FURI_LOG_D(
            TAG,
            "Block %d: addr %04X size %04X at %d",
            xex->block_count,
            block->addr,
            block->size,
            block->offset);

        if(!storage_file_seek(xex->file, block->size, false)) {
            return false;
        }

        offset += block->size;
        ++xex->block_count;
    }

    return true;
}

void xex_file_close(XexFile* xex) {
    if(xex) {
        if(xex->file) {
            storage_file_free(xex->file);
        }

        if(xex->name) {
            furi_string_free(xex->name);
        }

        free(xex);
    }
}

const char* xex_file_name(XexFile* xex) {
    furi_check(xex != NULL);
    return furi_string_get_cstr(xex->name);
}

size_t xex_file_size(XexFile* xex) {
    furi_check(xex != NULL);
    return xex->file_size;
}

uint16_t xex_file_block_count(XexFile* xex) {
    furi_check(xex != NULL);
    return xex->block_count;
}

bool xex_file_get_block(XexFile* xex, uint16_t block_idx, XexBlock* block) {
    furi_check(xex != NULL);
    furi_check(block != NULL);

    if(block_idx >= xex->block_count) {
        memset(block, 0, sizeof(XexBlock));
        return false;
    }

    *block = xex->blocks[block_idx];
    return true;
}

size_t xex_file_read(
    XexFile* xex,
    uint16_t block_idx,
    uintptr_t offset,
    void* buffer,
    size_t buffer_size) {
    furi_check(xex != NULL);
    furi_check(buffer != NULL);

    if(block_idx >= xex->block_count) {
        return 0;
    }

    XexBlock* block = &xex->blocks[block_idx];

    if(offset >= block->size) {
        return 0;
    }

    if(!storage_file_seek(xex->file, block->offset + offset, true)) {
        return 0;
    }

    size_t bytes_to_read = MIN(buffer_size, block->size - offset);

    int retval = storage_file_read(xex->file, buffer, bytes_to_read);

    return retval < 0 ? 0 : retval;
}

bool xex_file_overlaps_with(XexFile* xex, uint16_t addr, size_t size) {
    furi_check(xex != NULL);

    for(uint16_t i = 0; i < xex->block_count; ++i) {
        XexBlock* block = &xex->blocks[i];

        if((addr < block->addr + block->size) && (addr + size > block->addr)) {
            return true;
        }
    }

    return false;
}
