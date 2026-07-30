/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/console/tty.h>
#include <stdint.h>
#include <esp_loader_io.h>
#include <esp_loader.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration populated from DTS by the driver - can be stored in flash */
typedef struct {
    const struct device    *uart_dev;
    struct gpio_dt_spec     enable_spec;
    struct gpio_dt_spec     boot_spec;
    uint32_t                baud_rate;
    uint32_t                baud_rate_high;
} esp_loader_config_t;

/**
 * @brief Zephyr UART port object managed by the esp-loader device driver.
 *
 * This struct is allocated and owned by the driver for each
 * @c espressif,esp-loader DTS node instance. Do not declare or initialise
 * it manually — obtain a pointer via @c esp_loader_interface_from_device().
 *
 * The driver is instantiated from a board overlay, e.g.:
 * @code{.dts}
 *   / {
 *       esp_loader0: esp_loader {
 *           compatible = "espressif,esp-loader";
 *           uart          = <&uart1>;
 *           default-baudrate  = <115200>;
 *           higher-baudrate   = <230400>;
 *           reset-gpios   = <&gpio0 25 (GPIO_PULL_UP)>;
 *           boot-gpios    = <&gpio0 26 (GPIO_PULL_UP)>;
 *           sync-timeout-ms = <100>;
 *           num-trials    = <10>;
 *           status = "okay";
 *       };
 *
 *       chosen { zephyr,esp-loader = &esp_loader0; };
 *   };
 * @endcode
 *
 * Typical usage:
 * @code{.c}
 *   const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_esp_loader));
 *
 *   esp_loader_t              *loader = esp_loader_from_device(dev);
 *   esp_loader_config_t       *config = esp_loader_config_from_device(dev);
 *   esp_loader_connect_args_t *args   = esp_loader_connect_args_from_device(dev);
 *
 *   esp_loader_connect(loader, args);
 *   esp_loader_change_transmission_rate(loader, intf->baud_rate_high);
 * @endcode
 */
typedef struct {
    esp_loader_port_t   port;
    const esp_loader_config_t *config;

    /* Private runtime state — do not access directly */
    uint64_t            _time_end;
    struct tty_serial   _tty;
    char                _tty_rx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
    char                _tty_tx_buf[CONFIG_ESP_SERIAL_FLASHER_UART_BUFSIZE];
} zephyr_port_t;

/** @brief Return the @c esp_loader_t handle owned by @p dev. */
esp_loader_t *esp_loader_from_device(const struct device *dev);

/** @brief Return the @c esp_loader_config_t owned by @p dev. */
const esp_loader_config_t *esp_loader_config_from_device(const struct device *dev);

/** @brief Return the connection arguments stored in @p dev config. */
const esp_loader_connect_args_t *esp_loader_connect_args_from_device(const struct device *dev);

/** @brief Set the host transmission rate */
esp_loader_error_t esp_loader_host_baudrate(esp_loader_t *loader, uint32_t baudrate);

#ifdef __cplusplus
}
#endif
