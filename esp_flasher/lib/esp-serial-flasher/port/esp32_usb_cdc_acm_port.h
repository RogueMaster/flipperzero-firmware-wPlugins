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
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "freertos/stream_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*loader_port_esp32_usb_cdc_acm_callback_t) (void);

typedef struct {
    uint16_t device_vid;
    uint16_t device_pid;
    uint32_t connection_timeout_ms;
    uint32_t out_buffer_size; /* Must be larger than max packet size */
    /* Only set needed callbacks, NULLed ones are ignored .The callbacks are called from
       the usb library task, so ensure operations done within are thread-safe */
    loader_port_esp32_usb_cdc_acm_callback_t acm_host_error_callback;
    loader_port_esp32_usb_cdc_acm_callback_t device_disconnected_callback;
    loader_port_esp32_usb_cdc_acm_callback_t acm_host_serial_state_callback;
} loader_esp32_usb_cdc_acm_config_t;

esp_loader_error_t loader_port_esp32_usb_cdc_acm_init(const loader_esp32_usb_cdc_acm_config_t *config);

esp_loader_error_t loader_port_esp32_usb_cdc_acm_deinit(void);

#ifdef __cplusplus
}
#endif
