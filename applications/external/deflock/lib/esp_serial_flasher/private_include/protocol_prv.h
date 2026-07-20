/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_loader.h"
#include "protocol.h"

typedef struct {
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

void log_loader_internal_error(error_code_t error);

esp_loader_error_t send_cmd(const send_cmd_config *config);
