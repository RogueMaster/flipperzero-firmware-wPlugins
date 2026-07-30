/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "slip.h"
#include "esp_loader.h"
#include "esp_loader_protocol.h"
#include "loader_log.h"

static const uint8_t DELIMITER = 0xC0;
static const uint8_t C0_REPLACEMENT[2] = {0xDB, 0xDC};
static const uint8_t DB_REPLACEMENT[2] = {0xDB, 0xDD};

static inline esp_loader_error_t peripheral_read(esp_loader_t *loader, uint8_t *buff, const size_t size)
{
    return loader->_port->ops->read(loader->_port, buff, size,
                                    loader->_port->ops->remaining_time(loader->_port));
}

static inline esp_loader_error_t peripheral_write(esp_loader_t *loader, const uint8_t *buff, const size_t size)
{
    return loader->_port->ops->write(loader->_port, buff, size,
                                     loader->_port->ops->remaining_time(loader->_port));
}


esp_loader_error_t SLIP_receive_packet(esp_loader_t *loader, uint8_t *buff, const size_t max_size, size_t *recv_size)
{
    uint8_t ch;

    // Wait for delimiter
    do {
        RETURN_ON_ERROR( peripheral_read(loader, &ch, 1) );
    } while (ch != DELIMITER);

    // Workaround: bootloader sends two dummy(0xC0) bytes after response when baud rate is changed.
    do {
        RETURN_ON_ERROR( peripheral_read(loader, &ch, 1) );
    } while (ch == DELIMITER);

    // Decode SLIP into buffer; ch already holds the first non-delimiter byte.
    for (size_t i = 0; i < max_size; ) {
        if (ch == 0xDB) {
            RETURN_ON_ERROR( peripheral_read(loader, &ch, 1) );
            if (ch == 0xDC) {
                buff[i++] = 0xC0;
            } else if (ch == 0xDD) {
                buff[i++] = 0xDB;
            } else {
                return ESP_LOADER_ERROR_INVALID_RESPONSE;
            }
        } else if (ch == DELIMITER) {
            *recv_size = i;
            LOADER_LOG_HEX(loader, "SERIAL RX", buff, *recv_size);
            return ESP_LOADER_SUCCESS;
        } else {
            buff[i++] = ch;
        }

        if (i < max_size) {
            RETURN_ON_ERROR( peripheral_read(loader, &ch, 1) );
        }
    }

    // Buffer full: drain to end delimiter so the next call sees a clean stream.
    do {
        RETURN_ON_ERROR( peripheral_read(loader, &ch, 1) );
    } while (ch != DELIMITER);

    *recv_size = max_size;
    LOADER_LOG_HEX(loader, "SERIAL RX", buff, *recv_size);

    return ESP_LOADER_SUCCESS;
}


esp_loader_error_t SLIP_send(esp_loader_t *loader, const uint8_t *data, const size_t size)
{
    LOADER_LOG_HEX(loader, "SERIAL TX", data, size);
    uint32_t to_write = 0;  // Bytes ready to write as they are
    uint32_t written = 0;   // Bytes already written

    for (uint32_t i = 0; i < size; i++) {
        if (data[i] != 0xC0 && data[i] != 0xDB) {
            to_write++; // Queue this byte for writing
            continue;
        }

        // We have a byte that needs encoding, write the queue first
        if (to_write > 0) {
            RETURN_ON_ERROR( peripheral_write(loader, &data[written], to_write) );
        }

        // Write the encoded byte
        if (data[i] == 0xC0) {
            RETURN_ON_ERROR( peripheral_write(loader, C0_REPLACEMENT, 2) );
        } else {
            RETURN_ON_ERROR( peripheral_write(loader, DB_REPLACEMENT, 2) );
        }

        // Update to start again after the encoded byte
        written = i + 1;
        to_write = 0;
    }

    // Write the rest of the bytes that didn't need encoding
    if (to_write > 0) {
        RETURN_ON_ERROR( peripheral_write(loader, &data[written], to_write) );
    }

    return ESP_LOADER_SUCCESS;
}


esp_loader_error_t SLIP_send_delimiter(esp_loader_t *loader)
{
    return peripheral_write(loader, &DELIMITER, 1);
}
