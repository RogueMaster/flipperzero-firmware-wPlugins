/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader_io.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete ESP32 SPI port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_spi(). Hardware initialisation is called automatically
 * inside esp_loader_init_spi() — no separate init step is needed.
 *
 * @code
 *   esp32_spi_port_t port = {
 *       .port.ops          = &esp32_spi_ops,
 *       .spi_bus           = SPI2_HOST,
 *       .frequency         = 20 * 1000000,
 *       .reset_pin         = GPIO_NUM_5,
 *       .spi_clk_pin       = GPIO_NUM_12,
 *       .spi_cs_pin        = GPIO_NUM_10,
 *       // ...
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_spi(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;            /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_spi() */
    spi_host_device_t spi_bus;
    uint32_t          frequency;
    gpio_num_t        spi_clk_pin;
    gpio_num_t        spi_miso_pin;
    gpio_num_t        spi_mosi_pin;
    gpio_num_t        spi_cs_pin;
    gpio_num_t        spi_quadwp_pin;
    gpio_num_t        spi_quadhd_pin;
    gpio_num_t        reset_pin;
    gpio_num_t        strap_bit0_pin;
    gpio_num_t        strap_bit1_pin;
    gpio_num_t        strap_bit2_pin;
    gpio_num_t        strap_bit3_pin;
    bool              dont_initialize_bus;  /*!< Set if bus already initialised externally */

    /* Private runtime state — do not access directly */
    spi_bus_config_t              _spi_config;
    spi_device_handle_t           _device_h;
    spi_device_interface_config_t _device_config;
    int64_t                       _time_end;
    bool                          _bus_needs_deinit;
} esp32_spi_port_t;

/** Port operations vtable for the ESP32 SPI port. */
extern const esp_loader_port_ops_t esp32_spi_ops;

#ifdef __cplusplus
}
#endif
