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

#include "esp32_sdio_port.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "esp_timer.h"
#include <unistd.h>

#define WORD_ALIGNED(ptr) ((size_t)ptr % sizeof(size_t) == 0)

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, uint16_t size, bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
}
#endif

static sdmmc_card_t s_card;
static sdmmc_host_t s_card_config;
static int64_t s_time_end;
static uint32_t s_reset_trigger_pin;
static uint32_t s_boot_pin;
static bool host_driver_needs_deinit;

esp_loader_error_t loader_port_esp32_sdio_init(const loader_esp32_sdio_config_t *config)
{
    /* Initialize the global static variables*/
    s_reset_trigger_pin = config->reset_trigger_pin;
    s_boot_pin = config->boot_pin;
    s_card_config = (sdmmc_host_t)SDMMC_HOST_DEFAULT();
    s_card_config.flags = SDMMC_HOST_FLAG_4BIT;
    s_card_config.max_freq_khz = config->max_freq_khz;
    s_card_config.slot = config->slot;

#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
    /*
        The buffer and lenght of the data passed to SDMMC Host Driver needs to be aligned.
        The new L1 cache requires 64 bytes aligning of a buffer,
        this flag ensures that SDMMC Driver allocates custom aligned buffer and memcpy the data to it.
        This should be reworked when the new sdmmc driver-ng comes out as it would not need same alignment for size and buffer.
    */
    s_card_config.flags = SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
#endif

    if (!config->dont_initialize_host_driver) {
        if (sdmmc_host_init() != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 4;
#if SOC_SDMMC_USE_GPIO_MATRIX
        slot_config.clk = config->sdio_clk_pin;
        slot_config.cmd = config->sdio_cmd_pin;
        slot_config.d0 = config->sdio_d0_pin;
        slot_config.d1 = config->sdio_d1_pin;
        slot_config.d2 = config->sdio_d2_pin;
        slot_config.d3 = config->sdio_d3_pin;
#endif

        if (sdmmc_host_init_slot(config->slot, &slot_config) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        host_driver_needs_deinit = true;
    }

    gpio_reset_pin(s_reset_trigger_pin);
    gpio_set_pull_mode(s_reset_trigger_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(s_reset_trigger_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(s_boot_pin);
    gpio_set_pull_mode(s_boot_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(s_boot_pin, GPIO_MODE_OUTPUT);

    return ESP_LOADER_SUCCESS;
}


void loader_port_esp32_sdio_deinit(void)
{
    if (host_driver_needs_deinit) {
#if SOC_CACHE_INTERNAL_MEM_VIA_L1CACHE
        free(s_card.host.dma_aligned_buffer);
#endif
        sdmmc_host_deinit();
        host_driver_needs_deinit = false;
    }
    gpio_reset_pin(s_reset_trigger_pin);
    gpio_reset_pin(s_boot_pin);
}


esp_loader_error_t loader_port_write(const uint32_t function, const uint32_t addr,
                                     const uint8_t *data, const uint16_t size,
                                     const uint32_t timeout)
{
    (void) timeout; // Unused, there is a global timeout in the ESP-IDF driver

    if (data == NULL || !WORD_ALIGNED(data)) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    esp_err_t err = sdmmc_io_write_bytes(&s_card, function, addr, data, (size_t)size);

    if (err == ESP_OK) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, true);
#endif
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}


esp_loader_error_t loader_port_read(const uint32_t function, const uint32_t addr, uint8_t *data,
                                    const uint16_t size, const uint32_t timeout)
{
    (void) timeout; // Unused, there is a global timeout in the ESP-IDF driver

    if (data == NULL || !WORD_ALIGNED(data)) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    esp_err_t err = sdmmc_io_read_bytes(&s_card, function, addr, data, (size_t)size);

    if (err == ESP_OK) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, false);
#endif
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

void loader_port_enter_bootloader(void)
{
    gpio_set_level(s_boot_pin, 0);
    loader_port_reset_target();
    loader_port_delay_ms(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    gpio_set_level(s_boot_pin, 1);
}


esp_loader_error_t loader_port_sdio_card_init(void)
{
    const esp_loader_error_t err = sdmmc_card_init(&s_card_config, &s_card);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

void loader_port_reset_target(void)
{
    gpio_set_level(s_reset_trigger_pin, 0);
    loader_port_delay_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_set_level(s_reset_trigger_pin, 1);
}


void loader_port_delay_ms(const uint32_t ms)
{
    usleep(ms * 1000);
}


void loader_port_start_timer(const uint32_t ms)
{
    s_time_end = esp_timer_get_time() + ms * 1000;
}


uint32_t loader_port_remaining_time(void)
{
    int64_t remaining = (s_time_end - esp_timer_get_time()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


void loader_port_debug_print(const char *str)
{
    printf("DEBUG: %s\n", str);
}

esp_loader_error_t loader_port_wait_int(uint32_t timeout)
{
    if (sdmmc_io_wait_int(&s_card, timeout) == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else {
        return ESP_LOADER_ERROR_TIMEOUT;
    }
}
