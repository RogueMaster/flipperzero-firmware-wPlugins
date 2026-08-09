/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "protocol_prv.h"
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Protocol operations vtable.
 *
 * Each protocol implementation (serial/SLIP, SPI, SDIO) provides a static const
 * instance of this struct. The core dispatcher in esp_loader.c calls through
 * these function pointers instead of using compile-time selected global symbols.
 *
 * NULL entries indicate unsupported operations for that protocol.
 */
typedef struct esp_loader_protocol_ops_s {
    esp_loader_error_t (*initialize_conn)(esp_loader_t *loader,
                                          esp_loader_connect_args_t *args);

    esp_loader_error_t (*send_cmd)(esp_loader_t *loader,
                                   const struct send_cmd_config *config);

    esp_loader_error_t (*spi_attach)(esp_loader_t *loader, uint32_t config);

    /* Raw stub packet transport used by shared flash-read stub logic. */
    esp_loader_error_t (*recv_stub_packet)(esp_loader_t *loader, uint8_t *dest,
                                           size_t max_size, size_t *recv_size);
    esp_loader_error_t (*send_stub_ack)(esp_loader_t *loader, uint32_t bytes_recv);

    /* SDIO overrides (NULL = use shared default from protocol_serial.c) */
    esp_loader_error_t (*mem_begin_cmd)(esp_loader_t *loader, uint32_t offset,
                                        uint32_t size, uint32_t blocks_to_write,
                                        uint32_t block_size);
    esp_loader_error_t (*mem_data_cmd)(esp_loader_t *loader,
                                       const uint8_t *data, uint32_t size);
    esp_loader_error_t (*mem_end_cmd)(esp_loader_t *loader, uint32_t entrypoint);
} esp_loader_protocol_ops_t;

/**
 * @brief Protocol ops getters.
 *
 * Each protocol source file defines one of these and returns a pointer to its
 * static const protocol ops instance.  @c esp_loader_init() calls the
 * appropriate getter based on the @c esp_loader_protocol_t enum value.
 *
 * Serial transports (UART, USB CDC-ACM, …) share the SLIP wire protocol.
 */
const esp_loader_protocol_ops_t *esp_loader_get_serial_ops(void);
const esp_loader_protocol_ops_t *esp_loader_get_spi_ops(void);
const esp_loader_protocol_ops_t *esp_loader_get_sdio_ops(void);

#ifdef __cplusplus
}
#endif
