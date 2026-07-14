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

#include "app/app_common.h"

#include "xex_loader.h"

#define CHUNK_SIZE 1024

#define ERROR_TITLE "Executable not loaded"

// XEX loader specific SIO commands
#define SIO_COMMAND_READ_BLOCK_HEADER 0xF0 // Read XEX block header
#define SIO_COMMAND_READ_BLOCK        0xF1 // Read XEX block

struct XexLoader {
    AppConfig* config;
    SIODriver* sio;
    XexFile* xex;
    size_t last_offset;
    XexActivityCallback activity_callback;
    void* callback_context;

    const uint8_t* boot_sys;
    size_t boot_sys_len;
};

static SIOStatus xex_loader_command_callback(void* context, SIORequest* request);
static SIOStatus xex_loader_data_callback(void* context, SIORequest* request);

XexLoader* xex_loader_alloc(AppConfig* config) {
    XexLoader* loader = (XexLoader*)malloc(sizeof(XexLoader));
    memset(loader, 0, sizeof(XexLoader));

    loader->config = config;

    return loader;
}

void xex_loader_free(XexLoader* loader) {
    if(loader != NULL) {
        xex_loader_stop(loader);
        xex_file_close(loader->xex);
        free(loader);
    }
}

void xex_loader_set_activity_callback(
    XexLoader* loader,
    XexActivityCallback activity_callback,
    void* context) {
    furi_check(loader != NULL);
    loader->activity_callback = activity_callback;
    loader->callback_context = context;
}

static void xex_loader_notify_activity(XexLoader* fdd) {
    furi_check(fdd != NULL);
    if(fdd->activity_callback) {
        fdd->activity_callback(fdd->callback_context);
    }
}

// Try to get binary to boot that does not overlap with the XEX file
static bool get_boot_sys(XexFile* xex, const uint8_t** boot_sys, size_t* boot_sys_len) {
    furi_check(xex != NULL);
    furi_check(boot_sys != NULL);
    furi_check(boot_sys_len != NULL);

    extern unsigned char xex_boot600_sys[];
    extern unsigned int xex_boot600_sys_len;
    extern unsigned char xex_boot700_sys[];
    extern unsigned int xex_boot700_sys_len;

    if(!xex_file_overlaps_with(xex, 0x700, xex_boot700_sys_len)) {
        *boot_sys = xex_boot700_sys;
        *boot_sys_len = xex_boot700_sys_len;
    } else if(!xex_file_overlaps_with(xex, 0x600, xex_boot600_sys_len)) {
        *boot_sys = xex_boot600_sys;
        *boot_sys_len = xex_boot600_sys_len;
    } else {
        set_last_error(ERROR_TITLE, "XEX file overlaps with boot binary");
        return false;
    }

    return true;
}

bool xex_loader_start(XexLoader* loader, XexFile* xex, SIODriver* sio) {
    furi_check(loader != NULL);

    loader->last_offset = 0;

    if(loader->xex != xex) {
        xex_file_close(loader->xex);
    }

    if(!get_boot_sys(xex, &loader->boot_sys, &loader->boot_sys_len)) {
        return false;
    }

    loader->xex = xex;

    if(loader->sio != NULL) {
        sio_driver_detach(loader->sio, SIO_DEVICE_DISK1);
    }

    if(!sio_driver_attach(
           sio, SIO_DEVICE_DISK1, xex_loader_command_callback, xex_loader_data_callback, loader)) {
        return false;
    }

    loader->sio = sio;
    return true;
}

void xex_loader_stop(XexLoader* loader) {
    furi_check(loader != NULL);

    if(loader->sio != NULL) {
        sio_driver_detach(loader->sio, SIO_DEVICE_DISK1);
        loader->sio = NULL;
    }

    if(loader->xex != NULL) {
        xex_file_close(loader->xex);
        loader->xex = NULL;
    }
}

XexFile* xex_loader_file(XexLoader* loader) {
    furi_check(loader != NULL);
    return loader->xex;
}

size_t xex_loader_last_offset(XexLoader* loader) {
    furi_check(loader != NULL);
    return loader->last_offset;
}

static SIOStatus xex_loader_command_callback(void* context, SIORequest* request) {
    XexLoader* loader = (XexLoader*)context;

    furi_check(loader != NULL);

    switch(request->command) {
    case SIO_COMMAND_STATUS:
        return SIO_ACK;

    case SIO_COMMAND_READ:
        return SIO_ACK;

    case SIO_COMMAND_READ_BLOCK_HEADER:
        return SIO_ACK;

    case SIO_COMMAND_READ_BLOCK:
        return SIO_ACK;

    default:
        return SIO_NAK;
    }
}

static SIOStatus xex_loader_data_callback(void* context, SIORequest* request) {
    XexLoader* loader = (XexLoader*)context;

    UNUSED(loader);

    switch(request->command) {
    case SIO_COMMAND_STATUS:
        request->tx_data[0] = 0x00;
        request->tx_data[1] = 0xFF;
        request->tx_data[2] = 0xE0;
        request->tx_data[3] = 0x00;
        request->tx_size = 4;
        // fdd_notify_activity(fdd, FddEmuActivity_Other, 0);
        return SIO_COMPLETE;

    case SIO_COMMAND_READ: {
        size_t sector = request->aux1 + request->aux2 * 256;

        request->tx_size = 128;

        memset(request->tx_data, 0, request->tx_size);

        if(sector >= 1 && sector <= (loader->boot_sys_len + 127) / 128) {
            uintptr_t offset = (sector - 1) * request->tx_size;
            size_t copysize = MIN(request->tx_size, loader->boot_sys_len - offset);
            FURI_LOG_D(TAG, "Copying %zu bytes from offset %zu", copysize, offset);
            memcpy(request->tx_data, &loader->boot_sys[offset], copysize);
        }
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_READ_BLOCK_HEADER: {
        uint8_t block_idx = request->aux1;

        XexBlock block;

        xex_file_get_block(loader->xex, block_idx, &block);

        FURI_LOG_D(TAG, "Block header: addr %04X size %04X", block.addr, block.size);

        request->tx_data[0] = block.addr & 0xFF;
        request->tx_data[1] = (block.addr >> 8) & 0xFF;
        request->tx_data[2] = block.size & 0xFF;
        request->tx_data[3] = (block.size >> 8) & 0xFF;

        request->tx_size = 4;

        xex_loader_notify_activity(loader);
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_READ_BLOCK: {
        uint8_t block_idx = request->aux1;
        uint8_t chunk_idx = request->aux2;

        size_t offset = chunk_idx * CHUNK_SIZE;

        XexBlock block;

        if(!xex_file_get_block(loader->xex, block_idx, &block)) {
            return SIO_ERROR;
        }

        if(offset >= block.size) {
            return SIO_ERROR;
        }

        request->tx_size =
            xex_file_read(loader->xex, block_idx, offset, request->tx_data, CHUNK_SIZE);

        loader->last_offset = block.offset + offset + request->tx_size;
        xex_loader_notify_activity(loader);
        return request->tx_size > 0 ? SIO_COMPLETE : SIO_ERROR;
    }

    default:
        return SIO_ERROR;
    }
}
