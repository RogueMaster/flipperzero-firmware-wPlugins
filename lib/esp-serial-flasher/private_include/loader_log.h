/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_loader_io.h"
#include "esp_loader.h"

static inline void loader_log_dispatch(esp_loader_t *loader,
                                       esp_loader_log_level_t level,
                                       const char *fmt, ...)
{
    if (loader->_port->ops->log) {
        va_list args;
        va_start(args, fmt);
        loader->_port->ops->log(loader->_port, level, fmt, args);
        va_end(args);
    }
}

#define LOADER_LOG_HEX_(loader, level, label, data, size) \
    do { \
        if ((loader)->_port->ops->log_hex) \
            (loader)->_port->ops->log_hex((loader)->_port, (level), (label), (data), (size)); \
    } while (0)

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_ERROR
#  define LOADER_LOGE(loader, ...) loader_log_dispatch(loader, ESP_LOADER_LOG_ERROR, __VA_ARGS__)
#else
#  define LOADER_LOGE(loader, ...) do {} while (0)
#endif

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_WARN
#  define LOADER_LOGW(loader, ...) loader_log_dispatch(loader, ESP_LOADER_LOG_WARN, __VA_ARGS__)
#else
#  define LOADER_LOGW(loader, ...) do {} while (0)
#endif

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_INFO
#  define LOADER_LOGI(loader, ...) loader_log_dispatch(loader, ESP_LOADER_LOG_INFO, __VA_ARGS__)
#else
#  define LOADER_LOGI(loader, ...) do {} while (0)
#endif

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_DEBUG
#  define LOADER_LOGD(loader, ...) loader_log_dispatch(loader, ESP_LOADER_LOG_DEBUG, __VA_ARGS__)
#  define LOADER_LOG_HEX(loader, label, data, size) \
       LOADER_LOG_HEX_(loader, ESP_LOADER_LOG_DEBUG, label, data, size)
#else
#  define LOADER_LOGD(loader, ...) do {} while (0)
#  define LOADER_LOG_HEX(loader, label, data, size) do {} while (0)
#endif
