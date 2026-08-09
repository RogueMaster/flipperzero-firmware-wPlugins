/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_loader.h"

typedef struct target_registers_t {
    uint32_t cmd;
    uint32_t usr;
    uint32_t usr1;
    uint32_t usr2;
    uint32_t w0;
    uint32_t mosi_dlen;
    uint32_t miso_dlen;
} target_registers_t;

/**
 * @brief Detect the connected target chip and populate ctx->target and ctx->reg.
 *
 * If ctx->target is already set (e.g. SDIO identified the chip during card init),
 * only the register table is looked up and GET_SECURITY_INFO is skipped.
 * Otherwise the full UART/SPI detection sequence is used.
 */
// ESP32-P4 chip revision v3.0 corresponds to ECO5; earlier silicon needs the rev1 stub.
#define ESP32P4_ECO_REV3_MIN 5

esp_loader_error_t loader_detect_chip(esp_loader_t *loader);
esp_loader_error_t loader_read_mac(esp_loader_t *loader, target_chip_t target_code, uint8_t *mac);
bool encryption_in_begin_flash_cmd(target_chip_t target);
esp_loader_error_t loader_read_spi_config(esp_loader_t *loader, target_chip_t target_chip, uint32_t *spi_config);
target_chip_t target_from_chip_id(uint32_t chip_id);
