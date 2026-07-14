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
#include "hardware/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_inst_t *uart_inst;
    uint baudrate;
    uint uart_rx_pin_num;
    uint uart_tx_pin_num;
    uint reset_trigger_pin_num;
    uint boot_pin_num;
    bool dont_initialize_peripheral; /* Use if the peripheral has already been initialized,
                                        useful when using the peripheral for multiple
                                        purposes (e.g. monitoring) */
} loader_pi_pico_config_t;

/**
  * @brief Initializes the serial interface.
  *
  * @param config[in] Port configuration data
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_FAIL Initialization failure
  */
esp_loader_error_t loader_port_pi_pico_init(const loader_pi_pico_config_t *config);

/**
  * @brief Deinitialize serial interface.
  */
void loader_port_pi_pico_deinit(void);

#ifdef __cplusplus
}
#endif
