/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader.h"
#include <stdint.h>
#include <stdlib.h>

esp_loader_error_t SLIP_receive_packet(esp_loader_t *loader, uint8_t *buff, size_t max_size, size_t *recv_size);

esp_loader_error_t SLIP_send(esp_loader_t *loader, const uint8_t *data, size_t size);

esp_loader_error_t SLIP_send_delimiter(esp_loader_t *loader);
