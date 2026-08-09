/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* Internal helpers for built-in port sources only; not part of the public API. */

#include "esp_loader_io.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void loader_port_stdio_log(esp_loader_port_t *port, esp_loader_log_level_t level,
                           const char *fmt, va_list args);

void loader_port_stdio_log_with_tag(esp_loader_port_t *port, esp_loader_log_level_t level,
                                    const char *fmt, va_list args, const char *tag);

void loader_port_stdio_log_hex(esp_loader_port_t *port, esp_loader_log_level_t level,
                               const char *label, const uint8_t *data, size_t size);

#define LOADER_PORT_STDIO_LOG_CALLBACK(callback_name, log_tag)                         \
    static void callback_name(esp_loader_port_t *port, esp_loader_log_level_t level,   \
                              const char *fmt, va_list args)                         \
    {                                                                                  \
        loader_port_stdio_log_with_tag(port, level, fmt, args, log_tag);               \
    }

#ifdef __cplusplus
}
#endif
