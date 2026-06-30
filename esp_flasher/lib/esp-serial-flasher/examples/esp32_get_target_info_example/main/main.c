/* Get target info example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <sys/param.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp32_port.h"
#include "esp_loader.h"
#include "example_common.h"

static const char *TAG = "serial_flasher";

#define HIGHER_BAUDRATE 230400

static const char *get_target_string(target_chip_t target)
{
    if (target >= ESP_MAX_CHIP) {
        return "INVALID_TARGET";
    }

    const char *target_to_string_mapping[ESP_MAX_CHIP] = {
        "ESP8266", "ESP32", "ESP32-S2",
        "ESP32-C3", "ESP32-S3", "ESP32-C2",
        "ESP32-C5", "ESP32-H2", "ESP32-C6"
    };

    return target_to_string_mapping[target];
}

void app_main(void)
{
    const loader_esp32_config_t config = {
        .baud_rate = 115200,
        .uart_port = UART_NUM_1,
        .uart_rx_pin = GPIO_NUM_5,
        .uart_tx_pin = GPIO_NUM_4,
        .reset_trigger_pin = GPIO_NUM_25,
        .gpio0_trigger_pin = GPIO_NUM_26,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "serial initialization failed.");
        return;
    }

    if (connect_to_target(HIGHER_BAUDRATE) == ESP_LOADER_SUCCESS) {

        uint32_t flash_size = 0;
        if (esp_loader_flash_detect_size(&flash_size) == ESP_LOADER_SUCCESS) {
            ESP_LOGI(TAG, "Target flash size [B]: %u", (unsigned int)flash_size);
        } else {
            ESP_LOGE(TAG, "Could not read flash size!");
        }

        uint8_t mac[6] = {0};
        if (esp_loader_read_mac(mac) == ESP_LOADER_SUCCESS) {
            ESP_LOGI(TAG, "Target WIFI MAC:");
            ESP_LOG_BUFFER_HEX(TAG, mac, sizeof(mac));
        } else {
            ESP_LOGE(TAG, "Could not read WIFI MAC!");
        }

        esp_loader_target_security_info_t security_info;
        esp_loader_error_t err = esp_loader_get_security_info(&security_info);
        if (err == ESP_LOADER_SUCCESS) {
            ESP_LOGI(TAG, "Target Security Information:");
            ESP_LOGI(TAG, "Target chip: %s", get_target_string(security_info.target_chip));
            if (security_info.target_chip != ESP32S2_CHIP) {
                ESP_LOGI(TAG, "Eco version number: %lu", (unsigned long)security_info.eco_version);
            }

            ESP_LOGI(TAG, "Secure boot: %s",
                     security_info.secure_boot_enabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Secure boot agressive revoke: %s",
                     security_info.secure_boot_enabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Flash encryption: %s",
                     security_info.flash_encryption_enabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Secure download mode: %s",
                     security_info.secure_download_mode_enabled ? "ENABLED" : "DISABLED");
            for (size_t key = 0; key < sizeof(security_info.secure_boot_revoked_keys); key++) {
                ESP_LOGI(TAG, "Secure boot key %d revoked: %s", key,
                         security_info.secure_boot_revoked_keys[key] ? "TRUE" : "FALSE");
            }

            const char *jtag_status_string;
            if (security_info.jtag_hardware_disabled) {
                jtag_status_string = "PERMANENTLY DISABLED";
            } else if (security_info.jtag_software_disabled) {
                jtag_status_string = "DISABLED IN SOFTWARE";
            } else {
                jtag_status_string = "ENABLED";
            }
            ESP_LOGI(TAG, "JTAG access: %s", jtag_status_string);

            ESP_LOGI(TAG, "USB access: %s",
                     !security_info.usb_disabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Flash encryption: %s",
                     security_info.flash_encryption_enabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Data cache in UART download mode: %s",
                     !security_info.dcache_in_uart_download_disabled ? "ENABLED" : "DISABLED");
            ESP_LOGI(TAG, "Instruction cache in UART download mode: %s",
                     !security_info.icache_in_uart_download_disabled ? "ENABLED" : "DISABLED");

        } else {
            ESP_LOGE(TAG, "Could not get security info!\n\
                Note that the ESP32 and ESP8266 do not support this command");
        }
    }
}
