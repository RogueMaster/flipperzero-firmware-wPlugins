/* Flashing multiple partitions over USB CDC ACM interface

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "esp_err.h"
#include "esp_loader.h"
#include "esp32_usb_cdc_acm_port.h"
#include "example_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

#define ESPRESSIF_VID 0x303a
#define ESP_SERIAL_JTAG_PID 0x1001

static const char *TAG = "usb_flasher";
static SemaphoreHandle_t device_disconnected_sem;

/**
 * @brief USB Host library handling task
 *
 * @param arg Unused
 */
static void usb_lib_task(void *arg)
{
    while (1) {
        /* Start handling system events */
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB: All devices freed");
            /* Continue handling USB events to allow device reconnection */
        }
    }
}

static void device_disconnected_callback(void)
{
    xSemaphoreGive(device_disconnected_sem);
}

/**
 * @brief Main application
 *
 * Here we open a connection with any ESP32-S3 device connected and try flashing it
 */
void app_main(void)
{
    /* Install USB Host driver. Should only be called once in entire application */
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    /* Create a task that will handle USB library events */
    BaseType_t task_created = xTaskCreate(usb_lib_task,
                                          "usb_lib",
                                          4096,
                                          xTaskGetCurrentTaskHandle(),
                                          20,
                                          NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing the USB CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    while (true) {
        const loader_esp32_usb_cdc_acm_config_t config = {
            .device_vid = ESPRESSIF_VID,
            .device_pid = ESP_SERIAL_JTAG_PID,
            .connection_timeout_ms = 1000,
            .out_buffer_size = 4096,
            .device_disconnected_callback = device_disconnected_callback,
        };

        ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", config.device_vid, config.device_pid);
        if (loader_port_esp32_usb_cdc_acm_init(&config) != ESP_LOADER_SUCCESS) {
            continue;
        }

        device_disconnected_sem = xSemaphoreCreateBinary();
        assert(device_disconnected_sem);

        /* The ESP32-S3 ignores the line coding set commands,
           so we don't set the higher baudrate argument */
        if (connect_to_target(0) == ESP_LOADER_SUCCESS) {
            example_binaries_t bin;
            get_example_binaries(esp_loader_get_target(), &bin);

            ESP_LOGI(TAG, "Loading bootloader...");
            flash_binary(bin.boot.data, bin.boot.size, bin.boot.addr);
            ESP_LOGI(TAG, "Loading partition table...");
            flash_binary(bin.part.data, bin.part.size, bin.part.addr);
            ESP_LOGI(TAG, "Loading app...");
            flash_binary(bin.app.data, bin.app.size, bin.app.addr);
            ESP_LOGI(TAG, "Done!");
        }

        /* Wait for device disconnection and start over */
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}
