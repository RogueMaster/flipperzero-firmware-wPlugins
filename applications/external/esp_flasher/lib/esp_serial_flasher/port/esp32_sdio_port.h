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

#include "esp_loader_io.h"
#include "driver/sdmmc_host.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* The SDIO slot to use, changes the pin mapping when SOC_SDMMC_USE_IOMUX is used. */
    int slot;
    /* Valid frequencies: round divisors of 40MHz.
       The frequency is limited by the GPIO matrix if it is used. */
    uint32_t max_freq_khz;
    /*
       SDIO pin values are used only when SOC_SDMMC_USE_GPIO_MATRIX is used.
       Otherwise, when using the IOMUX signal routing, they are predefined.
    */
    uint8_t sdio_clk_pin;
    uint8_t sdio_d0_pin;
    uint8_t sdio_d1_pin;
    uint8_t sdio_d2_pin;
    uint8_t sdio_d3_pin;
    uint8_t sdio_cmd_pin;
    /* Reset and strapping pins */
    uint8_t reset_trigger_pin;
    uint8_t boot_pin;
    /* Use when you want to initialize and deinitialize the host driver yourself,
       for instance when you have multiple devices on the bus.
       All configuration values except reset and strapping pins are unused if set.*/
    bool dont_initialize_host_driver;
} loader_esp32_sdio_config_t;

/**
  * @brief Initializes the SDIO interface.
  *
  * @param config[in] Configuration structure
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_FAIL Initialization failure
  */
esp_loader_error_t loader_port_esp32_sdio_init(const loader_esp32_sdio_config_t *config);

/**
  * @brief Deinitializes the SDIO interface.
  */
void loader_port_esp32_sdio_deinit(void);

esp_loader_error_t loader_port_wait_int(uint32_t timeout);

#ifdef __cplusplus
}
#endif
