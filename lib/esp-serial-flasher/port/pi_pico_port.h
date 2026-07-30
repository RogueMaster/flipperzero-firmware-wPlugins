/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader_io.h"
#include "hardware/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete Raspberry Pi Pico UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_serial(). Hardware initialisation is called automatically
 * inside esp_loader_init_serial() — no separate init step is needed.
 *
 * @code
 *   pi_pico_port_t port = {
 *       .port.ops             = &pi_pico_uart_ops,
 *       .uart_inst            = uart1,
 *       .baudrate             = 115200,
 *       .uart_rx_pin_num      = 21,
 *       .uart_tx_pin_num      = 20,
 *       .reset_pin_num        = 19,
 *       .boot_pin_num         = 18,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_serial(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;              /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_serial() */
    uart_inst_t *uart_inst;
    uint          baudrate;
    uint          uart_rx_pin_num;
    uint          uart_tx_pin_num;
    uint          reset_pin_num;
    uint          boot_pin_num;
    bool          dont_initialize_peripheral; /*!< Set if UART already initialised externally */

    /* Private runtime state — do not access directly */
    uint32_t _time_end;
    bool     _peripheral_needs_deinit;
} pi_pico_port_t;

/** Port operations vtable for the Raspberry Pi Pico UART port. */
extern const esp_loader_port_ops_t pi_pico_uart_ops;

#ifdef __cplusplus
}
#endif
