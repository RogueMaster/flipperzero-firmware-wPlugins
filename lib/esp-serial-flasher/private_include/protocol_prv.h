/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_loader.h"
#include "protocol.h"

typedef struct send_cmd_config {
    const void *cmd;
    size_t cmd_size;
    const void *data; // Set to NULL if the command has no data
    size_t data_size;
    void *resp_data; // Set to NULL if the response has no data
    size_t resp_data_size;
    uint32_t *resp_data_recv_size; /* Out parameter indicating actual size of the response read
                                      for commands where response size can vary, in which
                                      case resp_data_size is the maximum response data size allowed.
                                      Set to NULL to require fixed response size of resp_data_size. */
    uint32_t *reg_value; // Out parameter for the READ_REG command, will return zero otherwise
} send_cmd_config;

void log_loader_internal_error(esp_loader_t *loader, error_code_t error);

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_DEBUG
const char *loader_command_name(command_t cmd);
#endif
