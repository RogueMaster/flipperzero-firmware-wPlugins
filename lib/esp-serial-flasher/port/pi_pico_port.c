/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "pi_pico_port.h"
#include "loader_port_stdio_log.h"

/* The driver returns a baudrate it managed to achieve which might not be the
 * exact baudrate requested. Tolerance is 1%. */
static bool baud_is_within_tolerance(const uint requested_baudrate, const uint got_baudrate)
{
    const float baudrate_error = ((float)((int)requested_baudrate - (int)got_baudrate)
                                  / (float)requested_baudrate
                                 ) * 100.0f;
    return (baudrate_error < 1.0f) ? true : false;
}

static esp_loader_error_t pi_pico_port_init(esp_loader_port_t *port)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);

    if (!p->dont_initialize_peripheral) {
        const uint got_baudrate = uart_init(p->uart_inst, p->baudrate);
        if (!baud_is_within_tolerance(p->baudrate, got_baudrate)) {
            return ESP_LOADER_ERROR_FAIL;
        }

        gpio_set_function(p->uart_rx_pin_num, GPIO_FUNC_UART);
        gpio_set_function(p->uart_tx_pin_num, GPIO_FUNC_UART);
        p->_peripheral_needs_deinit = true;
    }

    gpio_init(p->reset_pin_num);
    gpio_set_dir(p->reset_pin_num, GPIO_OUT);

    gpio_init(p->boot_pin_num);
    gpio_set_dir(p->boot_pin_num, GPIO_OUT);

    return ESP_LOADER_SUCCESS;
}

static void pi_pico_port_deinit(esp_loader_port_t *port)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);

    if (p->_peripheral_needs_deinit) {
        uart_deinit(p->uart_inst);
        gpio_deinit(p->uart_rx_pin_num);
        gpio_deinit(p->uart_tx_pin_num);
        p->_peripheral_needs_deinit = false;
    }

    gpio_deinit(p->reset_pin_num);
    gpio_deinit(p->boot_pin_num);
}

static esp_loader_error_t pi_pico_uart_write(esp_loader_port_t *port, const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());

    size_t pos = 0;
    while (pos < size) {
        if (to_ms_since_boot(get_absolute_time()) - start_ms > timeout) {
            break;
        }

        if (uart_is_writable(p->uart_inst)) {
            uart_putc_raw(p->uart_inst, data[pos]);
            pos++;
        }
    }

    return (pos == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}

static esp_loader_error_t pi_pico_uart_read(esp_loader_port_t *port, uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());

    size_t pos = 0;
    while (pos < size) {
        if (to_ms_since_boot(get_absolute_time()) - start_ms > timeout) {
            break;
        }

        if (uart_is_readable(p->uart_inst)) {
            data[pos] = uart_getc(p->uart_inst);
            pos++;
        }
    }

    return (pos == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}

static void pi_pico_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    sleep_ms(ms);
}

static void pi_pico_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    p->_time_end = to_ms_since_boot(get_absolute_time()) + ms;
}

static uint32_t pi_pico_uart_remaining_time(esp_loader_port_t *port)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    int32_t remaining = (int32_t)(p->_time_end - to_ms_since_boot(get_absolute_time()));
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void pi_pico_uart_reset_target(esp_loader_port_t *port)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    gpio_put(p->reset_pin_num, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    sleep_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_put(p->reset_pin_num, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
}

static void pi_pico_uart_enter_bootloader(esp_loader_port_t *port)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    gpio_put(p->boot_pin_num, SERIAL_FLASHER_BOOT_INVERT ? 1 : 0);
    gpio_put(p->reset_pin_num, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    sleep_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_put(p->reset_pin_num, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
    sleep_ms(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    gpio_put(p->boot_pin_num, SERIAL_FLASHER_BOOT_INVERT ? 0 : 1);
}

static esp_loader_error_t pi_pico_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    pi_pico_port_t *p = container_of(port, pi_pico_port_t, port);
    const uint got_baudrate = uart_set_baudrate(p->uart_inst, baudrate);

    if (!baud_is_within_tolerance(baudrate, got_baudrate)) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

const esp_loader_port_ops_t pi_pico_uart_ops = {
    .init                     = pi_pico_port_init,
    .deinit                   = pi_pico_port_deinit,
    .enter_bootloader         = pi_pico_uart_enter_bootloader,
    .reset_target             = pi_pico_uart_reset_target,
    .start_timer              = pi_pico_uart_start_timer,
    .remaining_time           = pi_pico_uart_remaining_time,
    .delay_ms                 = pi_pico_uart_delay_ms,
    .log                      = loader_port_stdio_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = pi_pico_uart_change_rate,
    .write                    = pi_pico_uart_write,
    .read                     = pi_pico_uart_read,
};
