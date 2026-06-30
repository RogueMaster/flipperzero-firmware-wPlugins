/*
 * This file is part of the 8-bit ATAR SIO Emulator for Flipper Zero
 * (https://github.com/cepetr/sio2flip).
 * Copyright (c) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <furi.h>

#include "app/app_common.h"

#include "disk_image.h"
#include "fdd_emulator.h"

// POKEY clock frequency [Hz]
// PAL: 1.7734MHz, NTSC: 1.7898MHz
#define POKEY_CLOCK 1780000

// XF551 high-speed mode baudrate
#define XF551_BAUDRATE 38400

// FDD emulator specific SIO commands
#define SIO_COMMAND_PUT              0x50 // Write sector without verification
#define SIO_COMMAND_FORMAT           0x21 // Format disk
#define SIO_COMMAND_FORMAT_MEDIUM    0x22 // Format disk (medium density)
#define SIO_COMMAND_GET_HSI          0x3F // Get high-speed index
#define SIO_COMMAND_READ_PERCOM      0x4E // Read PERCOM configuration
#define SIO_COMMAND_WRITE_PERCOM     0x4F // Write PERCOM configuration
#define SIO_COMMAND_FORMAT_WITH_SKEW 0x66 // Format disk with skew
#define SIO_COMMAND_READ_HS          0xD2 // Read sector
#define SIO_COMMAND_WRITE_HS         0xD7 // Write sector
#define SIO_COMMAND_STATUS_HS        0xD3 // Get status
#define SIO_COMMAND_PUT_HS           0xD0 // Write sector without verification
#define SIO_COMMAND_FORMAT_HS        0xA1 // Format disk
#define SIO_COMMAND_READ_PERCOM_HS   0xCE // Read PERCOM configuration
#define SIO_COMMAND_WRITE_PERCOM_HS  0xCF // Write PERCOM configuration

static SIOStatus fdd_command_callback(void* context, SIORequest* request);
static SIOStatus fdd_data_callback(void* context, SIORequest* request);

struct FddEmulator {
    SIODriver* sio;

    SIODevice device;
    DiskImage* image;

    FddActivityCallback activity_callback;
    void* callback_context;

    // Sector accessed by the last host command
    size_t last_sector;

    // Flag indicating that disk geometry has changed by host
    bool geometry_changed;
    // Disk geometry used for PERCOM config and format commands
    DiskGeometry geometry;

    AppConfig* config;
};

FddEmulator* fdd_alloc(SIODevice device, AppConfig* config) {
    FddEmulator* fdd = (FddEmulator*)malloc(sizeof(FddEmulator));
    memset(fdd, 0, sizeof(FddEmulator));

    fdd->device = device;
    fdd->config = config;
    fdd->sio = NULL;

    return fdd;
}

void fdd_free(FddEmulator* fdd) {
    if(fdd != NULL) {
        fdd_stop(fdd);
        disk_image_close(fdd->image);
        free(fdd);
    }
}

bool fdd_start(FddEmulator* fdd, SIODriver* sio) {
    furi_check(fdd != NULL);

    if(fdd->sio != NULL) {
        sio_driver_detach(fdd->sio, fdd->device);
    }

    if(sio_driver_attach(sio, fdd->device, fdd_command_callback, fdd_data_callback, fdd)) {
        fdd->sio = sio;
        return true;
    }

    return false;
}

void fdd_stop(FddEmulator* fdd) {
    furi_check(fdd != NULL);

    if(fdd->sio != NULL) {
        sio_driver_detach(fdd->sio, fdd->device);
        fdd->sio = NULL;
    }
}

void fdd_set_activity_callback(
    FddEmulator* fdd,
    FddActivityCallback activity_callback,
    void* context) {
    furi_check(fdd != NULL);
    fdd->activity_callback = activity_callback;
    fdd->callback_context = context;
}

// Returns the last accessed sector
size_t fdd_get_last_sector(FddEmulator* fdd) {
    furi_check(fdd != NULL);
    return fdd->last_sector;
}

static void fdd_notify_activity(FddEmulator* fdd, FddActivity activity, uint16_t sector) {
    furi_check(fdd != NULL);
    fdd->last_sector = sector;
    if(fdd->activity_callback) {
        fdd->activity_callback(fdd->callback_context, fdd->device, activity, sector);
    }
}

DiskImage* fdd_get_disk(FddEmulator* fdd) {
    furi_check(fdd != NULL);
    return fdd->image;
}

bool fdd_insert_disk(FddEmulator* fdd, DiskImage* image) {
    furi_check(fdd != NULL);
    furi_check(image != NULL);

    if(image != fdd->image) {
        disk_image_close(fdd->image);
        fdd->image = image;
        fdd->geometry = disk_geometry(image);
        fdd->geometry_changed = false;
    }

    return true;
}

void fdd_eject_disk(FddEmulator* fdd) {
    furi_check(fdd != NULL);

    if(fdd->image) {
        disk_image_close(fdd->image);
        fdd->image = NULL;
    }
}

void fdd_swap_disk(FddEmulator* fdd, FddEmulator* other_fdd) {
    furi_check(fdd != NULL);
    furi_check(other_fdd != NULL);

    DiskImage* tmp = fdd->image;
    fdd->image = other_fdd->image;
    other_fdd->image = tmp;

    DiskGeometry tmp_geom = fdd->geometry;
    fdd->geometry = other_fdd->geometry;
    other_fdd->geometry = tmp_geom;

    bool tmp_geom_changed = fdd->geometry_changed;
    fdd->geometry_changed = other_fdd->geometry_changed;
    other_fdd->geometry_changed = tmp_geom_changed;

    size_t tmp_last_sector = fdd->last_sector;
    fdd->last_sector = other_fdd->last_sector;
    other_fdd->last_sector = tmp_last_sector;
}

SIODevice fdd_get_device(FddEmulator* fdd) {
    furi_check(fdd != NULL);
    return fdd->device;
}

static DiskGeometry decode_percom_config(const uint8_t* rx_data) {
    FURI_LOG_I(
        TAG,
        "PERCOM: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        rx_data[0],
        rx_data[1],
        rx_data[2],
        rx_data[3],
        rx_data[4],
        rx_data[5],
        rx_data[6],
        rx_data[7],
        rx_data[8],
        rx_data[9],
        rx_data[10],
        rx_data[11]);

    DiskGeometry geom = {0};
    geom.tracks = rx_data[0];
    geom.sectors_per_track = (rx_data[2] << 8) | rx_data[3];
    geom.heads = rx_data[4] + 1;
    geom.density = rx_data[5];
    geom.sector_size = (rx_data[6] << 8) | rx_data[7];
    return geom;
}

static SIOStatus fdd_command_callback(void* context, SIORequest* request) {
    FddEmulator* fdd = (FddEmulator*)context;
    furi_check(fdd != NULL);

    if(fdd->image == NULL) {
        return SIO_NAK; // !@# TODO??
    }

    switch(request->command) {
    case SIO_COMMAND_STATUS_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        // Fall through
    case SIO_COMMAND_STATUS:
        return SIO_ACK;

    case SIO_COMMAND_READ_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        // Fall through
    case SIO_COMMAND_READ: {
        size_t sector = request->aux1 + request->aux2 * 256;
        size_t sector_size = disk_image_nth_sector_size(fdd->image, sector);
        return sector_size == 0 ? SIO_NAK : SIO_ACK;
    }

    case SIO_COMMAND_PUT_HS:
    case SIO_COMMAND_WRITE_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        // Fall through
    case SIO_COMMAND_PUT:
    case SIO_COMMAND_WRITE: {
        size_t sector = request->aux1 + request->aux2 * 256;
        size_t sector_size = disk_image_nth_sector_size(fdd->image, sector);
        if(sector_size == 0) {
            return SIO_NAK;
        } else {
            request->rx_size = sector_size;
            return SIO_ACK;
        }
    }

    case SIO_COMMAND_READ_PERCOM_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        // Fall through
    case SIO_COMMAND_READ_PERCOM:
        return SIO_ACK;

    case SIO_COMMAND_WRITE_PERCOM_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        // Fall through
    case SIO_COMMAND_WRITE_PERCOM:
        request->rx_size = 12;
        return SIO_ACK;

    case SIO_COMMAND_GET_HSI:
        if(fdd->config->speed_mode == SpeedMode_USDoubler) {
            return SIO_ACK;
        } else {
            return SIO_NAK;
        }

    case SIO_COMMAND_FORMAT:
        return SIO_ACK;

    case SIO_COMMAND_FORMAT_HS:
        if(fdd->config->speed_mode != SpeedMode_XF551) {
            return SIO_NAK;
        }
        request->baudrate = XF551_BAUDRATE;
        return SIO_ACK;

    case SIO_COMMAND_FORMAT_WITH_SKEW:
        request->rx_size = 128;
        return SIO_ACK;

    case SIO_COMMAND_FORMAT_MEDIUM:
        return SIO_ACK;

    default:
        return SIO_NAK;
    }
}

static SIOStatus fdd_data_callback(void* context, SIORequest* request) {
    FddEmulator* fdd = (FddEmulator*)context;
    furi_check(fdd != NULL);

    if(fdd->image == NULL) {
        return SIO_ERROR; // !@# TODO???
    }

    switch(request->command) {
    case SIO_COMMAND_STATUS:
    case SIO_COMMAND_STATUS_HS: {
        DiskGeometry geom = disk_geometry(fdd->image);
        bool write_protect = disk_image_get_write_protect(fdd->image);

        uint8_t drive_status = 0;
        drive_status |= false ? 0x01 : 0x00; // Command frame error
        drive_status |= false ? 0x02 : 0x00; // Checksum error
        drive_status |= false ? 0x04 : 0x00; // Operation error
        drive_status |= write_protect ? 0x08 : 0x00; // Write protected
        drive_status |= 0x10; // Motor ON
        drive_status |= geom.sector_size == 256 ? 0x20 : 0x00; // Double density
        drive_status |= geom.heads == 2 ? 0x40 : 0x00; // Double sided
        drive_status |= geom.sectors_per_track == 26 ? 0x80 : 0x00; // Enhanced density

        request->tx_data[0] = drive_status;
        request->tx_data[1] = 0xFF;
        request->tx_data[2] = 0xE0;
        request->tx_data[3] = 0x00;
        request->tx_size = 4;
        fdd_notify_activity(fdd, FddEmuActivity_Other, 0);
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_READ:
    case SIO_COMMAND_READ_HS: {
        size_t sector = request->aux1 + request->aux2 * 256;
        if(!disk_image_read_sector(fdd->image, sector, request->tx_data)) {
            return SIO_ERROR;
        }

        fdd_notify_activity(fdd, FddEmuActivity_Read, sector);

        request->tx_size = disk_image_nth_sector_size(fdd->image, sector);
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_PUT:
    case SIO_COMMAND_PUT_HS:
    case SIO_COMMAND_WRITE:
    case SIO_COMMAND_WRITE_HS: {
        size_t sector = request->aux1 + request->aux2 * 256;

        if(disk_image_get_write_protect(fdd->image)) {
            return SIO_ERROR;
        }

        fdd_notify_activity(fdd, FddEmuActivity_Write, sector);

        if(!disk_image_write_sector(fdd->image, sector, request->rx_data)) {
            return SIO_ERROR;
        }
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_READ_PERCOM_HS:
    case SIO_COMMAND_READ_PERCOM: {
        DiskGeometry geom = fdd->geometry;
        request->tx_data[0] = geom.tracks;
        request->tx_data[1] = 1;
        request->tx_data[2] = geom.sectors_per_track >> 8;
        request->tx_data[3] = geom.sectors_per_track & 0xFF;
        request->tx_data[4] = geom.heads - 1;
        request->tx_data[5] = geom.density;
        request->tx_data[6] = geom.sector_size >> 8;
        request->tx_data[7] = geom.sector_size & 0xFF;
        request->tx_data[8] = 0xFF;
        request->tx_data[9] = 0;
        request->tx_data[10] = 0;
        request->tx_data[11] = 0;
        request->tx_size = 12;
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_WRITE_PERCOM_HS:
    case SIO_COMMAND_WRITE_PERCOM: {
        DiskGeometry geom = decode_percom_config(request->rx_data);
        fdd->geometry = geom;
        fdd->geometry_changed = true;
        return SIO_COMPLETE;
    }

    case SIO_COMMAND_FORMAT:
    case SIO_COMMAND_FORMAT_HS:
        if(disk_image_get_write_protect(fdd->image)) {
            return SIO_ERROR;
        }

        if(!fdd->geometry_changed) {
            // Default single-sided, single-density - 90KB disk
            if(!determine_disk_geometry(&fdd->geometry, 90 * 1024, 128)) {
                return SIO_ERROR;
            }
        }

        if(!disk_image_format(fdd->image, fdd->geometry)) {
            return SIO_ERROR;
        }

        fdd->geometry_changed = false;

        // Send a list of bad sectors terminated by 0xFFFF
        request->tx_size = disk_image_sector_size(fdd->image);
        memset(request->tx_data, 0xFF, request->tx_size);
        return SIO_COMPLETE;

    case SIO_COMMAND_FORMAT_MEDIUM:
        if(disk_image_get_write_protect(fdd->image)) {
            return SIO_ERROR;
        }
        if(!determine_disk_geometry(&fdd->geometry, 130 * 1024, 128)) {
            return SIO_ERROR;
        }
        if(!disk_image_format(fdd->image, fdd->geometry)) {
            return SIO_ERROR;
        }

        fdd->geometry_changed = false;

        // Send a list of bad sectors terminated by 0xFFFF
        request->tx_size = disk_image_sector_size(fdd->image);
        memset(request->tx_data, 0xFF, request->tx_size);
        return SIO_COMPLETE;

    case SIO_COMMAND_FORMAT_WITH_SKEW:
        // First 12 bytes contains percom configuration
        // Next bytes contains sector numbers (we ignore them)
        DiskGeometry geom = decode_percom_config(request->rx_data);
        fdd->geometry = geom;
        fdd->geometry_changed = true;

        if(!disk_image_format(fdd->image, fdd->geometry)) {
            return SIO_ERROR;
        }

        fdd->geometry_changed = false;

        // Send a list of bad sectors terminated by 0xFFFF
        request->tx_size = disk_image_sector_size(fdd->image);
        memset(request->tx_data, 0xFF, request->tx_size);
        return SIO_COMPLETE;

    case SIO_COMMAND_GET_HSI:
        request->tx_data[0] = fdd->config->speed_index;
        request->tx_size = 1;
        request->baudrate = POKEY_CLOCK / 2 / (7 + request->tx_data[0]);
        return SIO_COMPLETE;

    default:
        return SIO_ERROR;
    }
}
