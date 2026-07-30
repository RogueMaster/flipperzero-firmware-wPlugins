// SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT

// auto-generated from esp-flasher-stub v0.8.0
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v0.8.0

#include "esp_stubs.h"

#if __STDC_VERSION__ >= 201112L
_Static_assert(ESP8266_CHIP == 0, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32_CHIP == 1, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32S2_CHIP == 2, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32C3_CHIP == 3, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32S3_CHIP == 4, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32C2_CHIP == 5, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32C5_CHIP == 6, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32H2_CHIP == 7, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32C6_CHIP == 8, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32P4_CHIP == 9, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP32C61_CHIP == 10, "Stub table order matches target_chip_t enumeration");
_Static_assert(ESP_MAX_CHIP == 11, "Stub table order matches target_chip_t enumeration");
#endif

extern const esp_stub_t esp_stub_esp8266;
extern const esp_stub_t esp_stub_esp32;
extern const esp_stub_t esp_stub_esp32s2;
extern const esp_stub_t esp_stub_esp32c3;
extern const esp_stub_t esp_stub_esp32s3;
extern const esp_stub_t esp_stub_esp32c2;
extern const esp_stub_t esp_stub_esp32c5;
extern const esp_stub_t esp_stub_esp32h2;
extern const esp_stub_t esp_stub_esp32c6;
extern const esp_stub_t esp_stub_esp32p4;
extern const esp_stub_t esp_stub_esp32c61;

const esp_stub_t *const esp_stub[ESP_MAX_CHIP] = {
    [ESP8266_CHIP] = &esp_stub_esp8266,
    [ESP32_CHIP] = &esp_stub_esp32,
    [ESP32S2_CHIP] = &esp_stub_esp32s2,
    [ESP32C3_CHIP] = &esp_stub_esp32c3,
    [ESP32S3_CHIP] = &esp_stub_esp32s3,
    [ESP32C2_CHIP] = &esp_stub_esp32c2,
    [ESP32C5_CHIP] = &esp_stub_esp32c5,
    [ESP32H2_CHIP] = &esp_stub_esp32h2,
    [ESP32C6_CHIP] = &esp_stub_esp32c6,
    [ESP32P4_CHIP] = &esp_stub_esp32p4,
    [ESP32C61_CHIP] = &esp_stub_esp32c61,
};
