/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32_spi_port.h"
#include "loader_port_stdio_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#include <stdio.h>
#include <unistd.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
#define DMA_CHAN SPI_DMA_CH_AUTO
#else
#define DMA_CHAN 1
#endif

#define WORD_ALIGNED(ptr) ((size_t)ptr % sizeof(size_t) == 0)

LOADER_PORT_STDIO_LOG_CALLBACK(esp32_spi_log, "esf-spi");

static esp_loader_error_t esp32_spi_port_init(esp_loader_port_t *port)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);

    if (!p->dont_initialize_bus) {
        p->_spi_config.mosi_io_num   = p->spi_mosi_pin;
        p->_spi_config.miso_io_num   = p->spi_miso_pin;
        p->_spi_config.sclk_io_num   = p->spi_clk_pin;
        p->_spi_config.quadwp_io_num = p->spi_quadwp_pin;
        p->_spi_config.quadhd_io_num = p->spi_quadhd_pin;
        p->_spi_config.max_transfer_sz = 4096 * 4;

        if (spi_bus_initialize(p->spi_bus, &p->_spi_config, DMA_CHAN) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        p->_bus_needs_deinit = true;
    }

    p->_device_config.clock_speed_hz = p->frequency;
    p->_device_config.spics_io_num   = -1; /* CS managed manually as GPIO */
    p->_device_config.flags          = SPI_DEVICE_HALFDUPLEX;
    p->_device_config.queue_size     = 16;

    if (spi_bus_add_device(p->spi_bus, &p->_device_config, &p->_device_h) != ESP_OK) {
        return ESP_LOADER_ERROR_FAIL;
    }

    gpio_reset_pin(p->reset_pin);
    gpio_set_pull_mode(p->reset_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->reset_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(p->reset_pin, 1);

    gpio_reset_pin(p->spi_cs_pin);
    gpio_set_pull_mode(p->spi_cs_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->spi_cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(p->spi_cs_pin, 1);

    return ESP_LOADER_SUCCESS;
}

static void esp32_spi_port_deinit(esp_loader_port_t *port)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    gpio_reset_pin(p->reset_pin);
    gpio_reset_pin(p->spi_cs_pin);
    spi_bus_remove_device(p->_device_h);
    if (p->_bus_needs_deinit) {
        spi_bus_free(p->spi_bus);
        p->_bus_needs_deinit = false;
    }
}

static void esp32_spi_set_cs(esp_loader_port_t *port, uint32_t level)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    gpio_set_level(p->spi_cs_pin, level);
}

static esp_loader_error_t esp32_spi_write(esp_loader_port_t *port, const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    (void)timeout;

    if (data == NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    spi_transaction_t transaction = {
        .tx_buffer = data,
        .rx_buffer = NULL,
        .length    = size * 8U,
        .rxlength  = 0,
    };

    esp_err_t err = spi_device_transmit(p->_device_h, &transaction);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static esp_loader_error_t esp32_spi_read(esp_loader_port_t *port, uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    (void)timeout;

    if (data == NULL || !WORD_ALIGNED(data)) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    spi_transaction_t transaction = {
        .tx_buffer = NULL,
        .rx_buffer = data,
        .rxlength  = size * 8,
    };

    esp_err_t err = spi_device_transmit(p->_device_h, &transaction);

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static void esp32_spi_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    usleep(ms * 1000);
}

static void esp32_spi_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    p->_time_end = esp_timer_get_time() + (int64_t)ms * 1000;
}

static uint32_t esp32_spi_remaining_time(esp_loader_port_t *port)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    int64_t remaining = (p->_time_end - esp_timer_get_time()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void esp32_spi_reset_target(esp_loader_port_t *port)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);
    gpio_set_level(p->reset_pin, 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpio_set_level(p->reset_pin, 1);
}

static void esp32_spi_enter_bootloader(esp_loader_port_t *port)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);

    /*
     * Strapping pins may overlap with target SPI pins (e.g. ESP32-C3 MISO
     * and strap_bit0).  Tear down the SPI bus, configure strapping pins as
     * GPIOs, perform the reset sequence, then restore the SPI bus.
     */
    spi_bus_remove_device(p->_device_h);
    spi_bus_free(p->spi_bus);

    gpio_reset_pin(p->strap_bit0_pin);
    gpio_set_pull_mode(p->strap_bit0_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->strap_bit0_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(p->strap_bit1_pin);
    gpio_set_pull_mode(p->strap_bit1_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->strap_bit1_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(p->strap_bit2_pin);
    gpio_set_pull_mode(p->strap_bit2_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->strap_bit2_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(p->strap_bit3_pin);
    gpio_set_pull_mode(p->strap_bit3_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->strap_bit3_pin, GPIO_MODE_OUTPUT);

    gpio_set_level(p->strap_bit0_pin, 1);
    gpio_set_level(p->strap_bit1_pin, 0);
    gpio_set_level(p->strap_bit2_pin, 0);
    gpio_set_level(p->strap_bit3_pin, 0);
    esp32_spi_reset_target(port);
    usleep(SERIAL_FLASHER_BOOT_HOLD_TIME_MS * 1000);
    gpio_set_level(p->strap_bit3_pin, 1);
    gpio_set_level(p->strap_bit0_pin, 0);

    gpio_reset_pin(p->strap_bit0_pin);
    gpio_reset_pin(p->strap_bit1_pin);
    gpio_reset_pin(p->strap_bit2_pin);
    gpio_reset_pin(p->strap_bit3_pin);

    spi_bus_initialize(p->spi_bus, &p->_spi_config, DMA_CHAN);
    spi_bus_add_device(p->spi_bus, &p->_device_config, &p->_device_h);
}

static esp_loader_error_t esp32_spi_change_rate(esp_loader_port_t *port, uint32_t frequency)
{
    esp32_spi_port_t *p = container_of(port, esp32_spi_port_t, port);

    if (spi_bus_remove_device(p->_device_h) != ESP_OK) {
        return ESP_LOADER_ERROR_FAIL;
    }

    uint32_t old_frequency = p->_device_config.clock_speed_hz;
    p->_device_config.clock_speed_hz = frequency;

    if (spi_bus_add_device(p->spi_bus, &p->_device_config, &p->_device_h) != ESP_OK) {
        p->_device_config.clock_speed_hz = old_frequency;
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

const esp_loader_port_ops_t esp32_spi_ops = {
    .init                     = esp32_spi_port_init,
    .deinit                   = esp32_spi_port_deinit,
    .enter_bootloader         = esp32_spi_enter_bootloader,
    .reset_target             = esp32_spi_reset_target,
    .start_timer              = esp32_spi_start_timer,
    .remaining_time           = esp32_spi_remaining_time,
    .delay_ms                 = esp32_spi_delay_ms,
    .log                      = esp32_spi_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = esp32_spi_change_rate,
    .write                    = esp32_spi_write,
    .read                     = esp32_spi_read,
    .spi_set_cs               = esp32_spi_set_cs,
};
