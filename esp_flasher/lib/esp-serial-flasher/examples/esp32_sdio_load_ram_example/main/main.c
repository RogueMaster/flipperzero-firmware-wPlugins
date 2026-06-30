/* Example of loading the program into RAM through SDIO

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
#include "esp32_sdio_port.h"
#include "esp_loader.h"
#include "example_common.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "sdio_ram_loader";

// Max line size
#define BUF_LEN 128
static uint8_t buf[BUF_LEN] = {0};

void slave_monitor(void *arg)
{
    // Initialize UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, GPIO_NUM_5, GPIO_NUM_6,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_LEN * 4, BUF_LEN * 4, 0, NULL, 0));

    while (1) {
        int rxBytes = uart_read_bytes(UART_NUM_2, buf, BUF_LEN, 100 / portTICK_PERIOD_MS);
        buf[rxBytes] = '\0';
        printf("%s", buf);
    }
}

void app_main(void)
{
    example_ram_app_binary_t bin;

    const loader_esp32_sdio_config_t config = {
        .slot = SDMMC_HOST_SLOT_1,
        .max_freq_khz = SDMMC_FREQ_DEFAULT,
        .reset_trigger_pin = GPIO_NUM_54,
        .boot_pin = GPIO_NUM_4,
        .sdio_d0_pin = GPIO_NUM_14,
        .sdio_d1_pin = GPIO_NUM_15,
        .sdio_d2_pin = GPIO_NUM_16,
        .sdio_d3_pin = GPIO_NUM_17,
        .sdio_clk_pin = GPIO_NUM_18,
        .sdio_cmd_pin = GPIO_NUM_19,
    };

    if (loader_port_esp32_sdio_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, " SDIO initialization failed.");
        abort();
    }

    if (connect_to_target(0) == ESP_LOADER_SUCCESS) {
        get_example_ram_app_binary(esp_loader_get_target(), &bin);
        ESP_LOGI(TAG, "Loading app to RAM ...");
        esp_loader_error_t err = load_ram_binary(bin.ram_app.data);
        loader_port_esp32_sdio_deinit();
        if (err == ESP_LOADER_SUCCESS) {
            // Forward slave's serial output
            ESP_LOGI(TAG, "********************************************");
            ESP_LOGI(TAG, "*** Logs below are print from slave .... ***");
            ESP_LOGI(TAG, "********************************************");
            xTaskCreate(slave_monitor, "slave_monitor", 2048, NULL, configMAX_PRIORITIES - 1, NULL);
        } else {
            ESP_LOGE(TAG, "Loading to RAM failed ...");
        }
    }
    vTaskDelete(NULL);
}
