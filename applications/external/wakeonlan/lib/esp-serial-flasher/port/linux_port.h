/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader_io.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Selects how the RESET and BOOT signals are driven.
 *
 * LINUX_GPIO_NONE
 *   No automatic GPIO control. The user must put the target into boot mode
 *   manually before calling esp_loader_connect().
 *
 * LINUX_GPIO_GPIOD
 *   Use the Linux GPIO character-device API via libgpiod (works on any SBC:
 *   Raspberry Pi, BeagleBone, NanoPi, …).
 *   Requires: libgpiod >= 2.0  (Debian 13 / Ubuntu 24.04 or later).
 *   Populate gpio_chip_path, reset_pin and boot_pin.
 *
 * LINUX_GPIO_DTR_RTS
 *   Drive RESET through the UART's RTS signal and BOOT through DTR.
 *   This is the standard esptool auto-reset circuit used on USB-to-UART
 *   adapter boards (CP2102, CH340, FT232, …).
 *   No extra config fields are needed beyond device and baudrate.
 *
 *   USB JTAG Serial devices (ESP32-C3/S3/C6/H2/P4 connected via their built-in
 *   USB, appearing as /dev/ttyACM*) are **auto-detected** within this mode via
 *   sysfs VID/PID lookup. The re-enumeration after reset is handled
 *   transparently — no extra flag or mode is needed.
 */
typedef enum {
    LINUX_GPIO_NONE,
    LINUX_GPIO_GPIOD,
    LINUX_GPIO_DTR_RTS,
} linux_gpio_mode_t;

/**
 * @brief Concrete Linux UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_serial(). Hardware initialisation is called automatically
 * inside esp_loader_init_serial() — no separate init step is needed.
 *
 * @code
 *   linux_port_t port = {
 *       .port.ops          = &linux_uart_ops,
 *       .device            = "/dev/ttyUSB0",
 *       .baudrate          = 115200,
 *       .gpio_mode         = LINUX_GPIO_DTR_RTS,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_serial(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;           /*!< Embedded port base — pass &port to esp_loader_init_* */

    /* Configuration — fill before calling esp_loader_init_serial() */
    const char       *device;         /*!< Serial device, e.g. "/dev/ttyUSB0" */
    uint32_t          baudrate;       /*!< Initial baud rate */
    linux_gpio_mode_t gpio_mode;      /*!< How RESET/BOOT GPIOs are driven */
    /**
     * Used only when gpio_mode == LINUX_GPIO_GPIOD.
     * Path to the GPIO chip (e.g. "/dev/gpiochip0") and line numbers.
     */
    const char       *gpio_chip_path;
    uint32_t          reset_pin;
    uint32_t          boot_pin;

    /* Private runtime state — do not access directly */
    int               _serial;
    int64_t           _time_end;
    bool              _is_usb_jtag; /*!< true when device is USB JTAG Serial (auto-detected) */
#if defined(LINUX_PORT_GPIO)
    struct gpiod_line_request *_gpio_request; /*!< libgpiod v2 bulk line request (reset + boot) */
#endif
} linux_port_t;

/** Port operations vtable for the Linux UART port. */
extern const esp_loader_port_ops_t linux_uart_ops;

#ifdef __cplusplus
}
#endif
