/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader_io.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SDIO_4BIT = 0,
    SDIO_1BIT,
} sdio_bus_width_t;

/**
 * @brief Concrete ESP32 SDIO port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_sdio(). Hardware initialisation is called automatically
 * inside esp_loader_init_sdio() — no separate init step is needed.
 *
 * @code
 *   esp32_sdio_port_t port = {
 *       .port.ops              = &esp32_sdio_ops,
 *       .slot                  = SDMMC_HOST_SLOT_1,
 *       .max_freq_khz          = SDMMC_FREQ_DEFAULT,
 *       .reset_pin             = GPIO_NUM_54,
 *       .boot_pin              = GPIO_NUM_53,
 *       .bus_width             = SDIO_4BIT,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_sdio(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t port;           /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_sdio() */
    int              slot;
    uint32_t         max_freq_khz;
    gpio_num_t       sdio_clk_pin;
    gpio_num_t       sdio_d0_pin;
    gpio_num_t       sdio_d1_pin;
    gpio_num_t       sdio_d2_pin;
    gpio_num_t       sdio_d3_pin;
    gpio_num_t       sdio_cmd_pin;
    gpio_num_t       reset_pin;
    gpio_num_t       boot_pin;
    bool             dont_initialize_host_driver;
    sdio_bus_width_t bus_width;

    /* Private runtime state — do not access directly */
    sdmmc_card_t  _card;
    sdmmc_host_t  _card_config;
    int64_t       _time_end;
    bool          _host_driver_needs_deinit;
} esp32_sdio_port_t;

/** Port operations vtable for the ESP32 SDIO port. */
extern const esp_loader_port_ops_t esp32_sdio_ops;

/**
 * @brief Wait for an SDIO interrupt from the target.
 *
 * @param port    Pointer to the SDIO port instance.
 * @param timeout Timeout in milliseconds.
 */
esp_loader_error_t loader_port_wait_int(esp32_sdio_port_t *port, uint32_t timeout);

#ifdef __cplusplus
}
#endif
