/* Copyright 2020-2025 Espressif Systems (Shanghai) CO LTD
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

// auto-generated stubs from esp-flasher-stub v0.3.0

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

bool esp_stub_get_running(void);
void esp_stub_set_running(bool stub_status);

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)

typedef struct {
    esp_loader_bin_header_t header;
    esp_loader_bin_segment_t segments[2];
} esp_stub_t;

extern const esp_stub_t esp_stub[ESP_MAX_CHIP];

#endif

#ifdef __cplusplus
}
#endif
