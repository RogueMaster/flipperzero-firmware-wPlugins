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

#include <furi_hal_resources.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_gpio.h>

#include "app/app_common.h"

#include "sio_driver.h"

#define MAX_SIO_DEVICES 8

#define SIO_DEFAULT_BAUDRATE 19200

// PC1 [pin 15] <- SIO COMMAND
#define GPIO_EXT_COMMAND  &gpio_ext_pc0
// PC0 [pin 16] <- SIO MOTORCTL
#define GPIO_EXT_MOTORCTL &gpio_ext_pc1

// PB6/USART1.TX [pin 13] -> SIO DIN
// PB7/USART1.RX [pin 14] -> SIO DOUT
#define GPIO_EXT_UART FuriHalSerialIdUsart

#define WORKER_EVT_STOP        (1 << 0)
#define WORKER_EVT_FRAME_RX    (1 << 1)
#define WORKER_EVT_FRAME_ERROR (1 << 2)
#define WORKER_EVT_STREAM_TX   (1 << 3)
#define WORKER_EVT_STREAM_RX   (1 << 4)

#define WORKER_EVT_ALL                                                                       \
    (WORKER_EVT_STOP | WORKER_EVT_FRAME_RX | WORKER_EVT_FRAME_ERROR | WORKER_EVT_STREAM_TX | \
     WORKER_EVT_STREAM_RX)

typedef struct {
    SIODevice device;
    SIOCallback command_callback;
    SIOCallback data_callback;
    void* callback_context;
} SIOHandler;

struct SIODriver {
    bool command_phase;
    size_t rx_count;
    size_t rx_expected;

    // Currently used baudrate
    uint32_t baudrate;
    // Baudrate for the next command
    uint32_t req_baudrate;
    // Alternative baudrate (used for baudrate swapping)
    uint32_t alt_baudrate;
    // Errors counter used for baudrate swapping
    uint32_t error_count;

    FuriHalSerialHandle* uart;
    FuriThread* rx_thread;
    SIORequest request;

    SIOTxCallback tx_callback;
    SIORxCallback rx_callback;
    void* callback_context;

    FuriStreamBuffer* rx_stream;
    bool stream_mode;

    SIOHandler handler[MAX_SIO_DEVICES];
};

static uint8_t calc_chksum(const uint8_t* data, size_t size) {
    uint32_t chksum = 0;
    for(size_t i = 0; i < size; i++) {
        chksum += data[i];
        if(chksum >= 0x100) {
            chksum = (chksum & 0xFF) + 1;
        }
    }
    return chksum;
}

static void sio_set_baudrate(SIODriver* sio, uint32_t baudrate) {
    furi_check(sio != NULL);

    if(baudrate != sio->baudrate) {
        sio->baudrate = baudrate;
        furi_hal_serial_tx_wait_complete(sio->uart);
        furi_hal_serial_set_br(sio->uart, sio->baudrate);
    }
}

static void sio_swap_baudrate(SIODriver* sio) {
    furi_check(sio != NULL);

    if(sio->baudrate == SIO_DEFAULT_BAUDRATE) {
        sio->req_baudrate = sio->alt_baudrate;
    } else {
        sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
    }
}

static void sio_driver_send_status(SIODriver* sio, uint8_t status) {
    if(status != SIO_NORESP) {
        FURI_LOG_I(TAG, "SIO: sending %02X status", status);
        furi_delay_ms(1);
        furi_hal_serial_tx(sio->uart, &status, sizeof(status));
    }
}

static void sio_driver_send_data(SIODriver* sio, const uint8_t* data, size_t size) {
    if(size > 0) {
        FURI_LOG_I(TAG, "SIO: sending %d bytes of data", sio->request.tx_size);
        furi_delay_ms(1);
        uint8_t chksum = calc_chksum(data, size);
        furi_hal_serial_tx(sio->uart, data, size);
        furi_hal_serial_tx(sio->uart, &chksum, 1);
    }
}

static SIOStatus sio_invoke_command_callback(SIODriver* sio, SIORequest* request) {
    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == request->device) {
            SIOCallback callback = handler->command_callback;
            void* context = handler->callback_context;
            if(callback) {
                return callback(context, request);
            }
        }
    }

    return SIO_NORESP;
}

static SIOStatus sio_invoke_data_callback(SIODriver* sio, SIORequest* request) {
    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == request->device) {
            SIOCallback callback = handler->data_callback;
            void* context = handler->callback_context;
            if(callback) {
                return callback(context, request);
            }
        }
    }

    return SIO_NORESP;
}

static void sio_driver_command_pin_callback(void* context) {
    SIODriver* sio = (SIODriver*)context;

    bool command_low = !furi_hal_gpio_read(GPIO_EXT_COMMAND);

    if(command_low) {
        sio->stream_mode = false;

        if(!sio->command_phase) {
            // Start of command
            sio->command_phase = true;
            sio->rx_count = 0;
            sio->rx_expected = 4; // Device, Command, Aux1, Aux2
        } else {
            // Command pin went log again during command phase
            furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_FRAME_ERROR);
        }

        if(sio->baudrate != sio->req_baudrate) {
            // Restore the requested baudrate
            sio_set_baudrate(sio, sio->req_baudrate);
        }
    }
}

void sio_driver_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    SIODriver* sio = (SIODriver*)context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t rx_byte = furi_hal_serial_async_rx(handle);

            if(sio->stream_mode) {
                furi_stream_buffer_send(sio->rx_stream, &rx_byte, sizeof(rx_byte), 0);
                furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_STREAM_RX);
                continue;
            }

            if(sio->rx_count < sio->rx_expected) {
                // Store received byte
                sio->request.rx_data[sio->rx_count++] = rx_byte;
            } else if(sio->rx_expected > 0 && sio->rx_count == sio->rx_expected) {
                // Checksum received
                sio->rx_expected = 0;

                if(rx_byte == calc_chksum(sio->request.rx_data, sio->rx_count)) {
                    // Checksum OK

                    if(sio->command_phase) {
                        sio->request.rx_size = 0;
                        sio->request.tx_size = 0;
                        sio->request.device = sio->request.rx_data[0];
                        sio->request.command = sio->request.rx_data[1];
                        sio->request.aux1 = sio->request.rx_data[2];
                        sio->request.aux2 = sio->request.rx_data[3];
                    } else {
                        sio->request.rx_size = sio->rx_count;
                    }
                    furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_FRAME_RX);

                } else {
                    // Checksum error
                    furi_thread_flags_set(
                        furi_thread_get_id(sio->rx_thread), WORKER_EVT_FRAME_ERROR);
                }
            }
        }
    }
}

static int32_t sio_driver_rx_worker(void* context) {
    SIODriver* sio = (SIODriver*)context;

    for(;;) {
        uint32_t events = furi_thread_flags_wait(WORKER_EVT_ALL, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WORKER_EVT_STOP) {
            // Worker thread is stopping
            break;
        }

        if(events & WORKER_EVT_FRAME_ERROR) {
            FURI_LOG_E(
                TAG,
                "SIO: frame error (%d bytes received - %02X %02X %02X %02X...)",
                sio->rx_count,
                sio->request.rx_data[0],
                sio->request.rx_data[1],
                sio->request.rx_data[2],
                sio->request.rx_data[3]);

            if(sio->command_phase) {
                sio->error_count++;
                if(sio->error_count >= 2) {
                    sio->error_count = 0;
                    // Swap baudrate after 2 consecutive errors
                    // (US Doubler high-speed mode)
                    sio_swap_baudrate(sio);
                }
            }

            sio->command_phase = false;
        }

        if(events & WORKER_EVT_FRAME_RX) {
            sio->error_count = 0;

            if(sio->command_phase) {
                FURI_LOG_I(
                    TAG,
                    "SIO: command frame received: %02X %02X %02X %02X",
                    sio->request.device,
                    sio->request.command,
                    sio->request.aux1,
                    sio->request.aux2);
            } else {
                FURI_LOG_I(TAG, "SIO: data frame received (%d bytes)", sio->rx_count);
            }

            if(sio->command_phase) {
                // Process the command frame
                sio->request.baudrate = sio->baudrate;
                uint8_t status = sio_invoke_command_callback(sio, &sio->request);

                __disable_irq();
                sio->rx_count = 0;
                sio->rx_expected = sio->request.rx_size;
                sio->command_phase = false;
                __enable_irq();

                // ACK or NAK the command frame
                sio_driver_send_status(sio, status);

                if(status == SIO_ACK) {
                    if(sio->request.baudrate != sio->baudrate) {
                        // Temporary baudrate for the data frame
                        // (XF551 high-speed mode)
                        sio->alt_baudrate = SIO_DEFAULT_BAUDRATE;
                        sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
                        sio_set_baudrate(sio, sio->request.baudrate);
                        sio->request.baudrate = SIO_DEFAULT_BAUDRATE;
                    }

                    if(sio->rx_expected == 0) {
                        // Execute the command without receiving data
                        uint8_t status = sio_invoke_data_callback(sio, &sio->request);

                        sio_driver_send_status(sio, status);
                        sio_driver_send_data(sio, sio->request.tx_data, sio->request.tx_size);

                        if(sio->request.baudrate != sio->req_baudrate) {
                            // Permanent baudrate change for all subsequent commands
                            // (US Doubler high-speed mode)
                            sio->alt_baudrate = sio->request.baudrate;
                            sio->req_baudrate = sio->request.baudrate;
                            FURI_LOG_D(TAG, "SIO: baudrate changed to %ld", sio->req_baudrate);
                        }
                    }
                }
            } else {
                // ACK data frame
                sio_driver_send_status(sio, SIO_ACK);

                // Execute the command with received data
                uint8_t status = sio_invoke_data_callback(sio, &sio->request);

                sio_driver_send_status(sio, status);
                sio_driver_send_data(sio, sio->request.tx_data, sio->request.tx_size);
            }
        }

        if(events & WORKER_EVT_STREAM_TX) {
            // Stream mode TX
            if(sio->stream_mode && sio->tx_callback != NULL) {
                uint8_t buffer[64];
                size_t size = sio->tx_callback(sio->callback_context, buffer, sizeof(buffer));
                while(size > 0) {
                    furi_hal_serial_tx(sio->uart, buffer, size);
                    size = sio->tx_callback(sio->callback_context, buffer, sizeof(buffer));
                }
            }
        }

        if(events & WORKER_EVT_STREAM_RX) {
            if(sio->stream_mode && sio->rx_callback != NULL) {
                uint8_t buffer[64];
                size_t size =
                    furi_stream_buffer_receive(sio->rx_stream, buffer, sizeof(buffer), 0);
                while(size > 0) {
                    sio->rx_callback(sio->callback_context, buffer, size);
                    size = furi_stream_buffer_receive(sio->rx_stream, buffer, sizeof(buffer), 0);
                }
            }
        }
    }
    return 0;
}

SIODriver* sio_driver_alloc() {
    SIODriver* sio = malloc(sizeof(SIODriver));
    memset(sio, 0, sizeof(SIODriver));

    furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeInterruptRiseFall, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(GPIO_EXT_MOTORCTL, GpioModeInput, GpioPullNo, GpioSpeedLow);

    sio->baudrate = SIO_DEFAULT_BAUDRATE;
    sio->req_baudrate = SIO_DEFAULT_BAUDRATE;
    sio->alt_baudrate = SIO_DEFAULT_BAUDRATE;
    sio->uart = furi_hal_serial_control_acquire(GPIO_EXT_UART);

    if(!sio->uart) {
        FURI_LOG_E(TAG, "Failed to acquire UART");
        goto cleanup;
    }

    furi_hal_serial_init(sio->uart, sio->baudrate);

    sio->rx_stream = furi_stream_buffer_alloc(2048, 1);
    if(sio->rx_stream == NULL) {
        FURI_LOG_E(TAG, "Failed to allocate RX stream buffer");
        goto cleanup;
    }

    sio->rx_thread = furi_thread_alloc();
    if(!sio->rx_thread) {
        FURI_LOG_E(TAG, "Failed to allocate RX thread");
        goto cleanup;
    }

    furi_thread_set_name(sio->rx_thread, "SIO RX");
    furi_thread_set_stack_size(sio->rx_thread, 4096);
    furi_thread_set_context(sio->rx_thread, sio);
    furi_thread_set_callback(sio->rx_thread, sio_driver_rx_worker);

    furi_thread_start(sio->rx_thread);

    furi_hal_serial_async_rx_start(sio->uart, sio_driver_rx_callback, sio, true);

    furi_hal_gpio_add_int_callback(GPIO_EXT_COMMAND, sio_driver_command_pin_callback, sio);

    FURI_LOG_I(TAG, "SIO driver initialized");

    return sio;

cleanup:
    sio_driver_free(sio);
    return NULL;
}

void sio_driver_free(SIODriver* sio) {
    if(sio != NULL) {
        furi_hal_gpio_remove_int_callback(GPIO_EXT_COMMAND);

        if(sio->uart != NULL) {
            furi_hal_serial_async_rx_stop(sio->uart);
        }

        if(sio->rx_thread != NULL) {
            furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_STOP);
            furi_thread_join(sio->rx_thread);
            furi_thread_free(sio->rx_thread);
        }

        if(sio->rx_stream != NULL) {
            furi_stream_buffer_free(sio->rx_stream);
        }

        if(sio->uart != NULL) {
            furi_hal_serial_deinit(sio->uart);
            furi_hal_serial_control_release(sio->uart);
        }

        free(sio);

        furi_hal_gpio_init(GPIO_EXT_COMMAND, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_gpio_init(GPIO_EXT_MOTORCTL, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

        FURI_LOG_I(TAG, "SIO driver deinitialized");
    }
}

bool sio_driver_attach(
    SIODriver* sio,
    SIODevice device,
    SIOCallback command_callback,
    SIOCallback data_callback,
    void* context) {
    furi_check(sio != NULL);
    furi_check(command_callback != NULL);
    furi_check(data_callback != NULL);

    // TODO: synchronize this with worker thread

    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->command_callback == NULL) {
            handler->device = device;
            handler->command_callback = command_callback;
            handler->data_callback = data_callback;
            handler->callback_context = context;
            return true;
        }
    }

    return false;
}

void sio_driver_detach(SIODriver* sio, uint8_t device) {
    furi_check(sio != NULL);

    // TODO: synchronize this with worker thread

    for(int i = 0; i < MAX_SIO_DEVICES; i++) {
        SIOHandler* handler = &sio->handler[i];
        if(handler->device == device) {
            handler->device = 0;
            handler->command_callback = NULL;
            handler->data_callback = NULL;
            handler->callback_context = NULL;
            break;
        }
    }
}

void sio_driver_set_stream_callbacks(
    SIODriver* sio,
    SIORxCallback rx_callback,
    SIOTxCallback tx_callback,
    void* context) {
    furi_check(sio != NULL);

    // TODO: synchronize this with worker thread

    sio->rx_callback = rx_callback;
    sio->tx_callback = tx_callback;
    sio->callback_context = context;
}

void sio_driver_activate_stream_mode(SIODriver* sio) {
    furi_check(sio != NULL);
    sio->stream_mode = true;
}

void sio_driver_wakeup_tx(SIODriver* sio) {
    furi_check(sio != NULL);
    furi_thread_flags_set(furi_thread_get_id(sio->rx_thread), WORKER_EVT_STREAM_TX);
}
