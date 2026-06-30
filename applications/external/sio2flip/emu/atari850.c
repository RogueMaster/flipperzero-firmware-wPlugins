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

#include "atari850.h"

// Atari 850 specific SIO commands
#define SIO_COMMAND_POLL      0x3F
#define SIO_COMMAND_BOOT      0x21
#define SIO_COMMAND_DOWNLOAD  0x26
#define SIO_COMMAND_CONTROL   0x41
#define SIO_COMMAND_SETBAUD   0x42
#define SIO_COMMAND_STARTCONC 0x58

#define ROM_SIZE             0x1000
#define ROM_BOOT_CODE_OFFSET 2321
#define ROM_BOOT_CODE_SIZE   384
#define ROM_R_HANDLER_OFFSET 2663
#define ROM_R_HANDLER_SIZE   1425

struct Atari850 {
    AppConfig* config;
    SIODriver* sio;
    Atari850ActivityCallback activity_callback;
    uint8_t* rom;
    void* callback_context;
};

static SIOStatus atari850_command_callback(void* context, SIORequest* request);
static SIOStatus atari850_data_callback(void* context, SIORequest* request);

Atari850* atari850_alloc(AppConfig* config) {
    Atari850* emu = (Atari850*)malloc(sizeof(Atari850));
    memset(emu, 0, sizeof(Atari850));

    emu->config = config;

    return emu;
}

void atari850_free(Atari850* emu) {
    if(emu != NULL) {
        atari850_stop(emu);
        free(emu->rom);
        free(emu);
    }
}

void atari850_load_rom(Atari850* emu, Storage* storage) {
    furi_check(emu != NULL);

    FURI_LOG_I(TAG, "Loading Atari 850 ROM...");

    File* file = storage_file_alloc(storage);
    furi_check(file != NULL, "Failed to allocate file");

    const char* file_name = APP_DATA_PATH("atari850.rom");

    if(storage_file_open(file, file_name, FSAM_READ, FSOM_OPEN_EXISTING)) {
        emu->rom = (uint8_t*)malloc(ROM_SIZE);

        if(storage_file_read(file, emu->rom, ROM_SIZE) != ROM_SIZE) {
            FURI_LOG_E(TAG, "Failed to read file %s", file_name);
            free(emu->rom);
            emu->rom = NULL;
        }

    } else {
        FURI_LOG_E(TAG, "Failed to open file %s", file_name);
    }

    storage_file_close(file);
    storage_file_free(file);
}

void atari850_set_activity_callback(
    Atari850* emu,
    Atari850ActivityCallback activity_callback,
    void* context) {
    furi_check(emu != NULL);
    emu->activity_callback = activity_callback;
    emu->callback_context = context;
}

static void atari850_notify_activity(Atari850* emu) {
    furi_check(emu != NULL);
    if(emu->activity_callback) {
        emu->activity_callback(emu->callback_context);
    }
}

bool atari850_start(Atari850* emu, SIODriver* sio) {
    furi_check(emu != NULL);

    if(emu->sio != NULL) {
        sio_driver_detach(emu->sio, SIO_DEVICE_RS232);
    }

    if(sio_driver_attach(
           sio, SIO_DEVICE_RS232, atari850_command_callback, atari850_data_callback, emu)) {
        emu->sio = sio;
        return true;
    }

    return false;
}

void atari850_stop(Atari850* emu) {
    furi_check(emu != NULL);
    if(emu->sio != NULL) {
        sio_driver_detach(emu->sio, SIO_DEVICE_RS232);
        emu->sio = NULL;
    }
}

// 12 bytes of data send to the host on poll command.
// Contains DCB data for loading R: device handler.
static const uint8_t poll_dcb[12] = {
    0x50, // DDEVIC
    0x01, // DUNIT
    0x21, // DCOMND
    0x40, // DSTATS
    0x00, // DBUFLO
    0x05, // DBUFHI
    0x01, // DTIMLO
    0x00, // DUNUSE
    ROM_BOOT_CODE_SIZE & 0xFF, // DBYTLO
    ROM_BOOT_CODE_SIZE >> 8, // DBYTHI
    0x00, // DAUX1
    0x00, // DAUX2
};

int const pokey_divisor = 0x28; // 19200Bd

static const uint8_t pokey_config[9] = {
    (pokey_divisor & 0xFF), // AUDF1
    0x00, // AUDC1
    (pokey_divisor >> 8), // AUDF2
    0x00, // AUDC2
    (pokey_divisor & 0xFF), // AUDF3
    0x00, // AUDC3
    (pokey_divisor >> 8), // AUDF4
    0x00, // AUDC4
    0x78, // AUDCTL <- CH1+CH2 & CH3+CH4 @ 1.79Mhz
};

static SIOStatus atari850_command_callback(void* context, SIORequest* request) {
    Atari850* emu = (Atari850*)context;

    furi_check(emu != NULL);

    switch(request->command) {
    case SIO_COMMAND_STATUS:
    case SIO_COMMAND_POLL:
    case SIO_COMMAND_BOOT:
    case SIO_COMMAND_CONTROL:
    case SIO_COMMAND_SETBAUD:
    case SIO_COMMAND_DOWNLOAD:
    case SIO_COMMAND_STARTCONC:
        return SIO_ACK;

    case SIO_COMMAND_WRITE:
        if(request->aux1 != 0) {
            request->rx_size = 64;
        }
        return SIO_ACK;

    default:
        return SIO_NAK;
    }
}

static SIOStatus atari850_data_callback(void* context, SIORequest* request) {
    Atari850* emu = (Atari850*)context;

    UNUSED(emu);

    switch(request->command) {
    case SIO_COMMAND_STATUS:
        request->tx_data[0] = 0x00;
        request->tx_data[1] = 0; //0b00110000; // CTS=1
        request->tx_size = 2;
        atari850_notify_activity(emu);
        return SIO_COMPLETE;

    case SIO_COMMAND_POLL:
        memcpy(request->tx_data, poll_dcb, sizeof(poll_dcb));
        request->tx_size = sizeof(poll_dcb);
        atari850_notify_activity(emu);
        return SIO_COMPLETE;

    case SIO_COMMAND_BOOT:
        if(emu->rom != NULL) {
            memcpy(request->tx_data, &emu->rom[ROM_BOOT_CODE_OFFSET], ROM_BOOT_CODE_SIZE);
            request->tx_size = ROM_BOOT_CODE_SIZE;
            atari850_notify_activity(emu);
            return SIO_COMPLETE;
        } else {
            return SIO_ERROR;
        }

    case SIO_COMMAND_DOWNLOAD:
        if(emu->rom != NULL) {
            memcpy(request->tx_data, &emu->rom[ROM_R_HANDLER_OFFSET], ROM_R_HANDLER_SIZE);
            request->tx_size = ROM_R_HANDLER_SIZE;
            atari850_notify_activity(emu);
            return SIO_COMPLETE;
        } else {
            return SIO_ERROR;
        }

    case SIO_COMMAND_CONTROL:
        return SIO_COMPLETE;

    case SIO_COMMAND_SETBAUD:
        return SIO_COMPLETE;

    case SIO_COMMAND_WRITE:
        // aux1 number of bytes??
        return SIO_COMPLETE;

    case SIO_COMMAND_STARTCONC:
        memcpy(request->tx_data, pokey_config, sizeof(pokey_config));
        request->tx_size = sizeof(pokey_config);
        sio_driver_activate_stream_mode(emu->sio);
        return SIO_COMPLETE;

    default:
        return SIO_ERROR;
    }
}
