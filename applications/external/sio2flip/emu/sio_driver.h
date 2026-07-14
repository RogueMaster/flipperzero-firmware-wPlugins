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

#include <toolbox/pipe.h>

#define SIO_MAX_FRAME_SIZE 2048U

// SIO status codes
typedef enum {
    SIO_NORESP = 0x00,
    SIO_ACK = 0x41, // 'A'
    SIO_NAK = 0x4E, // 'N'
    SIO_COMPLETE = 0x43, // 'C'
    SIO_ERROR = 0x45, // 'E'
} SIOStatus;

// SIO device ids
typedef enum {
    SIO_DEVICE_DISK1 = 0x31,
    SIO_DEVICE_DISK2 = 0x32,
    SIO_DEVICE_DISK3 = 0x33,
    SIO_DEVICE_DISK4 = 0x34,
    SIO_DEVICE_RS232 = 0x50,
} SIODevice;

typedef uint8_t SIOCommand;

// Common SIO command codes
#define SIO_COMMAND_READ   0x52
#define SIO_COMMAND_WRITE  0x57
#define SIO_COMMAND_STATUS 0x53

// SIO request structure passed to the device driver
typedef struct {
    // Device id
    SIODevice device;
    // Command
    SIOCommand command;
    // Auxilary data (command specific)
    uint8_t aux1;
    uint8_t aux2;

    // Received data (set by device driver)
    uint8_t rx_data[SIO_MAX_FRAME_SIZE];
    // Received data size (set by device driver)
    size_t rx_size;

    // Data to send (set by device driver)
    uint8_t tx_data[SIO_MAX_FRAME_SIZE];
    // Size of data to send (set by device driver)
    size_t tx_size;

    // baudrate for data frame
    uint32_t baudrate;

} SIORequest;

// Callback for processing SIO requests
//
// NOTE: The callback is invoked from the SIO driver thread.
//
// There are two phases in SIO request processing:
//
//   Command frame phase:
//    - Triggered when a command frame is received.
//    - The callback must respond quickly, within 16ms.
//    - The callback should process the command, aux1, and aux2.
//    - Optionally, the callback can set `rx_size` to receive data from the host.
//    - Optionally, the callback can update `baudrate` for the data frame phase.
//    - The callback should return either `SIO_ACK` or `SIO_NAK`.
//
//   Data frame phase:
//    - Triggered when a data frame is received or sent.
//    - The callback must process the received data or prepare data to send.
//    - Optionally, the callback can set `tx_data` and `tx_size`.
//    - The callback should return `SIO_COMPLETE` or `SIO_ERROR`.
typedef SIOStatus (*SIOCallback)(void* context, SIORequest* request);

// SIO driver
typedef struct SIODriver SIODriver;

// Allocates an SIO driver and initializes the UART
SIODriver* sio_driver_alloc(void);

// Frees the SIO driver
void sio_driver_free(SIODriver* sio);

// Attaches a peripheral driver to the SIO driver
bool sio_driver_attach(
    SIODriver* sio,
    SIODevice device,
    SIOCallback command_callback,
    SIOCallback data_callback,
    void* context);

// Detaches a periheral driver from the SIO driver
void sio_driver_detach(SIODriver* sio, uint8_t device);

// Stream mode callbacks
typedef void (*SIORxCallback)(void* context, const void* data, size_t size);
typedef size_t (*SIOTxCallback)(void* context, void* data, size_t size);

// Sets callbacks for the stream mode. In this mode, the SIO driver
// will not process SIO requests, but will pass the received data
// to the `rx_callback` and will send data using the provided by
// `tx_callback`.
void sio_driver_set_stream_callbacks(
    SIODriver* sio,
    SIORxCallback rx_callback,
    SIOTxCallback tx_callback,
    void* context);

// Enables the stream mode. Stream mode will be automatically
// disable when a command frame is received.
void sio_driver_activate_stream_mode(SIODriver* sio);

// Wake up the SIO worker thread to call the `tx_callback`
// and send data to the UART.
void sio_driver_wakeup_tx(SIODriver* sio);
