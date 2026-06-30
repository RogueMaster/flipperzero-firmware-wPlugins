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

#include <stdio.h>
#include "hardware/gpio.h"
#include "pico/time.h"
#include "pi_pico_port.h"

static uart_inst_t *s_uart_inst;
static uint s_uart_rx_pin_num;
static uint s_uart_tx_pin_num;
static uint s_reset_trigger_pin_num;
static uint s_boot_pin_num;
static bool s_peripheral_needs_deinit;

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

static uint32_t s_time_end;

// The driver returns a baudrate it managed to achieve which might not be the
// exact baudrate we asked for. This is fine as long as the tolerance is low.
// We are setting it at 1%.
static bool baud_is_within_tolerance(const uint requested_baudrate, const uint got_baudrate)
{
    const float baudrate_error = ((float)((int)requested_baudrate - (int)got_baudrate)
                                  / (float)requested_baudrate
                                 ) * 100.0f;

    return (baudrate_error < 1.0f) ? true : false;
}

esp_loader_error_t loader_port_write(const uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    const uint32_t deadline_ms = to_ms_since_boot(get_absolute_time()) + timeout;

    size_t pos = 0;
    while (pos < size) {
        if (to_ms_since_boot(get_absolute_time()) > deadline_ms) {
            break;
        }

        if (uart_is_writable(s_uart_inst)) {
            uart_putc_raw(s_uart_inst, data[pos]);
            pos++;
        }
    }

#if SERIAL_FLASHER_DEBUG_TRACE
    transfer_debug_print(data, pos, true);
#endif

    return (pos == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}


esp_loader_error_t loader_port_read(uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    const uint32_t deadline_ms = to_ms_since_boot(get_absolute_time()) + timeout;

    size_t pos = 0;
    while (pos < size) {
        if (to_ms_since_boot(get_absolute_time()) > deadline_ms) {
            break;
        }

        if (uart_is_readable(s_uart_inst)) {
            data[pos] = uart_getc(s_uart_inst);
            pos++;
        }
    }

#if SERIAL_FLASHER_DEBUG_TRACE
    transfer_debug_print(data, pos, false);
#endif

    return (pos == size) ? ESP_LOADER_SUCCESS : ESP_LOADER_ERROR_TIMEOUT;
}


esp_loader_error_t loader_port_pi_pico_init(const loader_pi_pico_config_t *config)
{
    if (!config->dont_initialize_peripheral) {
        const uint got_baudrate = uart_init(config->uart_inst, config->baudrate);

        if (!baud_is_within_tolerance(config->baudrate, got_baudrate)) {
            return ESP_LOADER_ERROR_FAIL;
        }

        gpio_set_function(config->uart_rx_pin_num, GPIO_FUNC_UART);
        gpio_set_function(config->uart_tx_pin_num, GPIO_FUNC_UART);
        s_peripheral_needs_deinit = true;
    }

    gpio_init(config->reset_trigger_pin_num);
    gpio_set_dir(config->reset_trigger_pin_num, GPIO_OUT);

    gpio_init(config->boot_pin_num);
    gpio_set_dir(config->boot_pin_num, GPIO_OUT);

    s_uart_inst = config->uart_inst;
    s_uart_rx_pin_num = config->uart_rx_pin_num;
    s_uart_tx_pin_num = config->uart_tx_pin_num;
    s_reset_trigger_pin_num = config->reset_trigger_pin_num;
    s_boot_pin_num = config->boot_pin_num;

    return ESP_LOADER_SUCCESS;
}


void loader_port_pi_pico_deinit(void)
{
    if (s_peripheral_needs_deinit) {
        uart_deinit(s_uart_inst);
        gpio_deinit(s_uart_rx_pin_num);
        gpio_deinit(s_uart_tx_pin_num);
        s_peripheral_needs_deinit = false;
    }

    gpio_deinit(s_reset_trigger_pin_num);
    gpio_deinit(s_boot_pin_num);
}

void loader_port_enter_bootloader(void)
{
    gpio_put(s_boot_pin_num, SERIAL_FLASHER_BOOT_INVERT ? 1 : 0);

    loader_port_reset_target();
    loader_port_delay_ms(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);

    gpio_put(s_boot_pin_num, SERIAL_FLASHER_BOOT_INVERT ? 0 : 1);
}


void loader_port_reset_target(void)
{
    gpio_put(s_reset_trigger_pin_num, SERIAL_FLASHER_RESET_INVERT ? 1 : 0);
    loader_port_delay_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    gpio_put(s_reset_trigger_pin_num, SERIAL_FLASHER_RESET_INVERT ? 0 : 1);
}


void loader_port_delay_ms(const uint32_t ms)
{
    sleep_ms(ms);
}


void loader_port_start_timer(const uint32_t ms)
{
    s_time_end = to_ms_since_boot(get_absolute_time()) + ms;
}


uint32_t loader_port_remaining_time(void)
{
    int32_t remaining = s_time_end - to_ms_since_boot(get_absolute_time());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


void loader_port_debug_print(const char *str)
{
    printf("DEBUG: %s\n", str);
}

esp_loader_error_t loader_port_change_transmission_rate(const uint32_t baudrate)
{
    const uint got_baudrate = uart_set_baudrate(s_uart_inst, baudrate);

    if (!baud_is_within_tolerance(baudrate, got_baudrate)) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}
