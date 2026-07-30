/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "loader_port_stdio_log.h"

#include <stdio.h>

static const char *const s_level_prefix[] = { "", "E", "W", "I", "D" };

void loader_port_stdio_log_with_tag(esp_loader_port_t *port, esp_loader_log_level_t level,
                                    const char *fmt, va_list args, const char *tag)
{
    (void)port;
    printf("[%s] %s: ", s_level_prefix[level], tag);
    vprintf(fmt, args);
    putchar('\n');
}

void loader_port_stdio_log(esp_loader_port_t *port, esp_loader_log_level_t level,
                           const char *fmt, va_list args)
{
    loader_port_stdio_log_with_tag(port, level, fmt, args, "esf");
}

void loader_port_stdio_log_hex(esp_loader_port_t *port, esp_loader_log_level_t level,
                               const char *label, const uint8_t *data, size_t size)
{
    (void)port;
    printf("[%s] esf: %s (%zu bytes):\n", s_level_prefix[level],
           label ? label : "hex", size);
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            putchar('\n');
        }
    }
    if (size % 16 != 0) {
        putchar('\n');
    }
}
