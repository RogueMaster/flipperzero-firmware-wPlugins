/* Flash multiple partitions example

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
static const char example_data[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                                   "Donec pretium sem et condimentum tincidunt.n "
                                   "Quisque tristique in enim elementum aliquet. "
                                   "Integer consequat sodales bibendum. "
                                   "Nam enim quam, tristique id dui ut, fermentum porta felis. "
                                   "Phasellus vulputate sem quis ligula egestas, sed ullamcorper eros tincidunt. "
                                   "Donec imperdiet ac urna in placerat. "
                                   "Praesent ultrices velit nulla, eu rutrum nisi maximus in."
                                   "Donec non ligula molestie, blandit massa vel, auctor ipsum. "
                                   "Nunc consectetur mi nulla, a ultricies odio vehicula a. "
                                   "Praesent hendrerit tellus nunc, a interdum lectus mollis eget. "
                                   "Nullam in felis vitae diam posuere dignissim quis non urna. "
                                   "Quisque elementum ante at sapien condimentum, sit amet ultricies tortor feugiat. "
                                   "Aliquam id mi at purus maximus lobortis. "
                                   "Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. "
                                   "Nunc consequat lorem turpis, vitae dignissim leo fringilla eget. "
                                   "In ornare convallis finibus. "
                                   "Morbi libero neque, pretium et mollis non, dictum eget felis. "
                                   "Ut mattis vitae urna id vulputate. "
                                   "Nam porttitor dolor diam, eu rutrum tortor auctor id...";
static uint8_t read_buf[sizeof(example_data)];

#define HIGHER_BAUDRATE 230400

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

        ESP_LOGI(TAG, "Loading example data");
        flash_binary((const uint8_t *)example_data, sizeof(example_data), 0x00000000);

        if (esp_loader_flash_read(read_buf, 0x00000000, sizeof(read_buf)) == ESP_LOADER_SUCCESS) {
            if (!memcmp(example_data, read_buf, sizeof(read_buf))) {
                ESP_LOGI(TAG, "Flash contents match example data");
            } else {
                ESP_LOGE(TAG, "Flash contents do not match example data");
                ESP_LOG_BUFFER_HEXDUMP("Programmed data: ", example_data, sizeof(read_buf), ESP_LOG_ERROR);
                ESP_LOG_BUFFER_HEXDUMP("Read data: ", read_buf, sizeof(read_buf), ESP_LOG_ERROR);
            }
        } else {
            ESP_LOGE(TAG, "Could not read from flash!");
        }


    }
    vTaskDelete(NULL);
}
