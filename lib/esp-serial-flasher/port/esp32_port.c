/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32_port.h"
#include "loader_port_stdio_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_idf_version.h"
#include <stdio.h>
#include <unistd.h>

static esp_loader_error_t esp32_port_init(esp_loader_port_t *port)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);

    p->_peripheral_needs_deinit = false;

    if (!p->dont_initialize_peripheral) {
        uart_config_t uart_config = {
            .baud_rate = (int)p->baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .source_clk = UART_SCLK_DEFAULT,
#endif
        };

        int rx_buffer_size = p->rx_buffer_size ? (int)p->rx_buffer_size : 400;
        int tx_buffer_size = p->tx_buffer_size ? (int)p->tx_buffer_size : 400;
        QueueHandle_t *uart_queue = p->uart_queue ? p->uart_queue : NULL;
        int queue_size = p->queue_size ? (int)p->queue_size : 0;

        // Sets pin current strength to 40 mA, because some pins default to 10 mA,
        // which is not enough when USB-UART is also present on the UART lines (at least 20 mA should be sufficient).
        if (gpio_set_drive_capability(p->uart_tx_pin, GPIO_DRIVE_CAP_3) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }
        if (uart_param_config((uart_port_t)p->uart_port, &uart_config) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }
        if (uart_set_pin((uart_port_t)p->uart_port, (int)p->uart_tx_pin, (int)p->uart_rx_pin,
                         UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }
        if (uart_driver_install((uart_port_t)p->uart_port, rx_buffer_size, tx_buffer_size,
                                queue_size, uart_queue, 0) != ESP_OK) {
            return ESP_LOADER_ERROR_FAIL;
        }

        p->_peripheral_needs_deinit = true;
    }

    gpio_reset_pin(p->reset_pin);
    gpio_set_pull_mode(p->reset_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->reset_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(p->boot_pin);
    gpio_set_pull_mode(p->boot_pin, GPIO_PULLUP_ONLY);
    gpio_set_direction(p->boot_pin, GPIO_MODE_OUTPUT);

    return ESP_LOADER_SUCCESS;
}


static void esp32_port_deinit(esp_loader_port_t *port)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);

    if (p->_peripheral_needs_deinit) {
        uart_driver_delete((uart_port_t)p->uart_port);
        p->_peripheral_needs_deinit = false;
    }
}


static esp_loader_error_t esp32_uart_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);

    uart_write_bytes((uart_port_t)p->uart_port, (const char *)data, size);
    esp_err_t err = uart_wait_tx_done((uart_port_t)p->uart_port, pdMS_TO_TICKS(timeout));

    if (err == ESP_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}


static esp_loader_error_t esp32_uart_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);

    int read = uart_read_bytes((uart_port_t)p->uart_port, data, size, pdMS_TO_TICKS(timeout));

    if (read < 0) {
        return ESP_LOADER_ERROR_FAIL;
    } else if (read < size) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_SUCCESS;
    }
}


static void esp32_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    usleep(ms * 1000);
}


static void esp32_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);
    p->_time_end = esp_timer_get_time() + (int64_t)ms * 1000;
}


static uint32_t esp32_uart_remaining_time(esp_loader_port_t *port)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);
    int64_t remaining = (p->_time_end - esp_timer_get_time()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


static void esp32_uart_reset_target(esp_loader_port_t *port)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);
    gpio_set_level(p->reset_pin, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpio_set_level(p->reset_pin, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
}


static void esp32_uart_enter_bootloader(esp_loader_port_t *port)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);
    gpio_set_level(p->boot_pin, SERIAL_FLASHER_BOOT_INVERT ? 1 : 0);
    gpio_set_level(p->reset_pin, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    usleep(SERIAL_FLASHER_RESET_HOLD_TIME_MS * 1000);
    gpio_set_level(p->reset_pin, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
    usleep(SERIAL_FLASHER_BOOT_HOLD_TIME_MS * 1000);
    gpio_set_level(p->boot_pin, SERIAL_FLASHER_BOOT_INVERT ? 0 : 1);
}


static esp_loader_error_t esp32_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    esp32_port_t *p = container_of(port, esp32_port_t, port);
    esp_err_t err = uart_set_baudrate((uart_port_t)p->uart_port, baudrate);
    return (err == ESP_OK) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_FAIL;
}


const esp_loader_port_ops_t esp32_uart_ops = {
    .init                     = esp32_port_init,
    .deinit                   = esp32_port_deinit,
    .enter_bootloader         = esp32_uart_enter_bootloader,
    .reset_target             = esp32_uart_reset_target,
    .start_timer              = esp32_uart_start_timer,
    .remaining_time           = esp32_uart_remaining_time,
    .delay_ms                 = esp32_uart_delay_ms,
    .log                      = loader_port_stdio_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = esp32_uart_change_rate,
    .write                    = esp32_uart_write,
    .read                     = esp32_uart_read,
};
