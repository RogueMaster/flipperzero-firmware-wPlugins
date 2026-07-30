/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_loader_error.h"

/**
 * @brief Log verbosity levels used by the log and log_hex port callbacks.
 *
 * The numeric defines are usable in preprocessor @c #if guards as well as
 * in C expressions.  The enum typedef provides type safety in function
 * signatures; each member is initialised from the corresponding define so
 * the two representations are always in sync.
 */
#define ESP_LOADER_LOG_NONE    0  /*!< All output suppressed */
#define ESP_LOADER_LOG_ERROR   1  /*!< Protocol errors, MD5 mismatch */
#define ESP_LOADER_LOG_WARN    2  /*!< Flash size fallback, SDIO retries (default) */
#define ESP_LOADER_LOG_INFO    3  /*!< Connect success, chip, flash size, write start */
#define ESP_LOADER_LOG_DEBUG   4  /*!< Per-command trace, hex dumps, SPI polling */

typedef enum {
    ESP_LOADER_LOG_LEVEL_NONE  = ESP_LOADER_LOG_NONE,
    ESP_LOADER_LOG_LEVEL_ERROR = ESP_LOADER_LOG_ERROR,
    ESP_LOADER_LOG_LEVEL_WARN  = ESP_LOADER_LOG_WARN,
    ESP_LOADER_LOG_LEVEL_INFO  = ESP_LOADER_LOG_INFO,
    ESP_LOADER_LOG_LEVEL_DEBUG = ESP_LOADER_LOG_DEBUG,
} esp_loader_log_level_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Recovers a pointer to the outer struct from a pointer to one of its members.
 *
 * This is the standard "inheritance by embedding" pattern used throughout the
 * Linux kernel.  Given a pointer @p ptr to member @p member inside a struct of
 * type @p type, this macro returns a pointer to the enclosing struct.
 *
 * Example:
 * @code
 *   typedef struct {
 *       // ... config / state fields ...
 *       esp_loader_port_t base;   // embedded handle
 *   } my_port_t;
 *
 *   static void my_start_timer(esp_loader_port_t *port, uint32_t ms)
 *   {
 *       my_port_t *p = container_of(port, my_port_t, base);
 *       p->time_end = get_time_ms() + ms;
 *   }
 * @endcode
 */
#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/**
 * @brief Forward declaration of the port base handle type.
 *
 * Full definition follows below.  The forward declaration is needed because
 * @c esp_loader_port_ops_t references @c esp_loader_port_t in its callback signatures.
 */
typedef struct esp_loader_port_s esp_loader_port_t;

/**
 * @brief Unified port operations vtable.
 *
 * Every callback receives an @c esp_loader_port_t * (self-pointer).
 * Use @c container_of to recover the full concrete port struct and access
 * per-instance state.
 *
 * Set unused function pointers to NULL:
 *  - @c init / @c deinit         — NULL if no hardware setup is needed
 *  - @c change_transmission_rate — NULL for SDIO (host driver manages speed)
 *  - @c write / @c read          — NULL for SDIO ports
 *  - @c spi_set_cs               — NULL for non-SPI ports
 *  - @c sdio_write / @c sdio_read / @c sdio_card_init — NULL for non-SDIO ports
 */
typedef struct {
    /**
     * Initializes the hardware peripheral (UART driver, GPIO pins, etc.).
     * Called automatically by esp_loader_init_*(). NULL if not needed.
     */
    esp_loader_error_t (*init)(esp_loader_port_t *port);

    /**
     * Deinitializes the hardware peripheral.
     * The caller is responsible for invoking this when the port is no longer needed.
     * NULL if not needed.
     */
    void (*deinit)(esp_loader_port_t *port);

    /** Asserts bootstrap pins to enter boot mode and toggles reset. */
    void (*enter_bootloader)(esp_loader_port_t *port);

    /** Toggles the reset pin. */
    void (*reset_target)(esp_loader_port_t *port);

    /** Starts a one-shot timeout timer. */
    void (*start_timer)(esp_loader_port_t *port, uint32_t ms);

    /**
     * Returns remaining milliseconds since the last start_timer call.
     * Returns 0 if the timer has elapsed.
     */
    uint32_t (*remaining_time)(esp_loader_port_t *port);

    /** Delays execution for the given number of milliseconds. */
    void (*delay_ms)(esp_loader_port_t *port, uint32_t ms);

    /**
     * Logs a formatted text message at the given level.
     * The format string follows printf conventions and does NOT include a trailing newline —
     * the implementation is responsible for adding one if needed.
     * Set to NULL to suppress all text output.
     */
    void (*log)(esp_loader_port_t *port, esp_loader_log_level_t level,
                const char *fmt, va_list args);

    /**
     * Logs a binary buffer dump at the given level.
     * @param label  Optional human-readable label (may be NULL).
     * @param data   Pointer to the raw bytes to display.
     * @param size   Number of bytes.
     * Set to NULL to suppress hex output.
     */
    void (*log_hex)(esp_loader_port_t *port, esp_loader_log_level_t level,
                    const char *label, const uint8_t *data, size_t size);

    /**
     * Changes the peripheral clock/baud rate.
     * Set to NULL when not supported (e.g. SDIO).
     */
    esp_loader_error_t (*change_transmission_rate)(esp_loader_port_t *port, uint32_t rate);

    /** Writes bytes to the peripheral (UART/USB/SPI). NULL for SDIO.
     *  Hardware/DMA alignment requirements must be handled by the port implementation. */
    esp_loader_error_t (*write)(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout);

    /** Reads bytes from the peripheral (UART/USB/SPI). NULL for SDIO.
     *  Hardware/DMA alignment requirements must be handled by the port implementation. */
    esp_loader_error_t (*read)(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout);

    /** Controls the SPI chip-select pin. NULL for non-SPI ports. */
    void (*spi_set_cs)(esp_loader_port_t *port, uint32_t level);

    /** Writes data over SDIO. NULL for non-SDIO ports.
     *  Hardware/DMA alignment requirements must be handled by the port implementation. */
    esp_loader_error_t (*sdio_write)(esp_loader_port_t *port, uint32_t function, uint32_t addr,
                                     const uint8_t *data, uint16_t size, uint32_t timeout);

    /** Reads data over SDIO. NULL for non-SDIO ports.
     *  Hardware/DMA alignment requirements must be handled by the port implementation. */
    esp_loader_error_t (*sdio_read)(esp_loader_port_t *port, uint32_t function, uint32_t addr,
                                    uint8_t *data, uint16_t size, uint32_t timeout);

    /** Initializes the SDIO card. NULL for non-SDIO ports. */
    esp_loader_error_t (*sdio_card_init)(esp_loader_port_t *port);
} esp_loader_port_ops_t;

/**
 * @brief Port handle.
 *
 * For Phase 1, declare a plain @c esp_loader_port_t with @c ops pointing at
 * your static ops table and pass its address to @c esp_loader_init_*().
 *
 * In Phase 2 (after static variable migration) embed this struct inside a
 * larger per-instance struct and use @c container_of inside callbacks to
 * recover the full struct.
 *
 * The library stores @c esp_loader_port_t * internally and dispatches all
 * operations through @c port->ops->fn(port, ...).
 */
struct esp_loader_port_s {
    const esp_loader_port_ops_t *ops;
};

#ifdef __cplusplus
}
#endif
