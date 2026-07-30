/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32_sdio_port.h"
#include "loader_port_stdio_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <unistd.h>

LOADER_PORT_STDIO_LOG_CALLBACK(esp32_sdio_log, "esf-sdio");

static esp_loader_error_t esp32_sdio_port_init(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);

    p->_card_config = (sdmmc_host_t)SDMMC_HOST_DEFAULT();
    p->_card_config.flags = p->bus_width == SDIO_1BIT ? SDMMC_HOST_FLAG_1BIT : SDMMC_HOST_FLAG_4BIT;
    p->_card_config.flags |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
    p->_card_config.max_freq_khz = p->max_freq_khz;
    p->_card_config.slot = p->slot;

    if (!p->dont_initialize_host_driver) {
        if (sdmmc_host_init() != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        slot_config.width = p->bus_width == SDIO_1BIT ? 1 : 4;
#if SOC_SDMMC_USE_GPIO_MATRIX
        slot_config.clk = p->sdio_clk_pin;
        slot_config.cmd = p->sdio_cmd_pin;
        slot_config.d0  = p->sdio_d0_pin;
        slot_config.d1  = p->sdio_d1_pin;
        slot_config.d2  = p->sdio_d2_pin;
        slot_config.d3  = p->sdio_d3_pin;
#endif

        if (sdmmc_host_init_slot(p->slot, &slot_config) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        p->_host_driver_needs_deinit = true;
    }

    gpio_reset_pin(p->reset_pin);
    gpio_set_pull_mode(p->reset_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->reset_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(p->boot_pin);
    gpio_set_pull_mode(p->boot_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->boot_pin, GPIO_MODE_OUTPUT);

    return ESP_LOADER_SUCCESS;
}

static void esp32_sdio_port_deinit(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);

    free(p->_card.host.dma_aligned_buffer);
    p->_card.host.dma_aligned_buffer = NULL;

    if (p->_host_driver_needs_deinit) {
        sdmmc_host_deinit();
        p->_host_driver_needs_deinit = false;
    }
    gpio_reset_pin(p->reset_pin);
    gpio_reset_pin(p->boot_pin);
}

static esp_loader_error_t esp32_sdio_write(esp_loader_port_t *port, uint32_t function, uint32_t addr,
        const uint8_t *data, uint16_t size, uint32_t timeout)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    (void)timeout;

    if (data == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    esp_err_t err = sdmmc_io_write_bytes(&p->_card, function, addr, data, (size_t)size);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static esp_loader_error_t esp32_sdio_read(esp_loader_port_t *port, uint32_t function, uint32_t addr,
        uint8_t *data, uint16_t size, uint32_t timeout)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    (void)timeout;

    if (data == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    esp_err_t err = sdmmc_io_read_bytes(&p->_card, function, addr, data, (size_t)size);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static esp_loader_error_t esp32_sdio_card_init(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    esp_err_t err = sdmmc_card_init(&p->_card_config, &p->_card);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static void esp32_sdio_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    usleep(ms * 1000);
}

static void esp32_sdio_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    p->_time_end = esp_timer_get_time() + (int64_t)ms * 1000;
}

static uint32_t esp32_sdio_remaining_time(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    int64_t remaining = (p->_time_end - esp_timer_get_time()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void esp32_sdio_reset_target(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    gpio_set_level(p->reset_pin, 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpio_set_level(p->reset_pin, 1);
}

static void esp32_sdio_enter_bootloader(esp_loader_port_t *port)
{
    esp32_sdio_port_t *p = container_of(port, esp32_sdio_port_t, port);
    gpio_set_level(p->boot_pin, 0);
    gpio_set_level(p->reset_pin, 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpio_set_level(p->reset_pin, 1);
    usleep(SERIAL_FLASHER_BOOT_HOLD_TIME_MS * 1000);
    gpio_set_level(p->boot_pin, 1);
}

esp_loader_error_t loader_port_wait_int(esp32_sdio_port_t *port, uint32_t timeout)
{
    if (sdmmc_io_wait_int(&port->_card, timeout) == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else {
        return ESP_LOADER_ERROR_TIMEOUT;
    }
}

const esp_loader_port_ops_t esp32_sdio_ops = {
    .init                     = esp32_sdio_port_init,
    .deinit                   = esp32_sdio_port_deinit,
    .enter_bootloader         = esp32_sdio_enter_bootloader,
    .reset_target             = esp32_sdio_reset_target,
    .start_timer              = esp32_sdio_start_timer,
    .remaining_time           = esp32_sdio_remaining_time,
    .delay_ms                 = esp32_sdio_delay_ms,
    .log                      = esp32_sdio_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = NULL,
    .sdio_write               = esp32_sdio_write,
    .sdio_read                = esp32_sdio_read,
    .sdio_card_init           = esp32_sdio_card_init,
};
