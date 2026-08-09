/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes returned by the esp-serial-flasher library.
 */
typedef enum {
    ESP_LOADER_SUCCESS,                /*!< Success */
    ESP_LOADER_ERROR_FAIL,             /*!< Unspecified error */
    ESP_LOADER_ERROR_TIMEOUT,          /*!< Timeout elapsed */
    ESP_LOADER_ERROR_IMAGE_SIZE,       /*!< Image size to flash is larger than flash size */
    ESP_LOADER_ERROR_INVALID_MD5,      /*!< Computed and received MD5 does not match */
    ESP_LOADER_ERROR_INVALID_PARAM,    /*!< Invalid parameter passed to function */
    ESP_LOADER_ERROR_INVALID_TARGET,   /*!< Connected target is invalid */
    ESP_LOADER_ERROR_UNSUPPORTED_CHIP, /*!< Attached chip is not supported */
    ESP_LOADER_ERROR_UNSUPPORTED_FUNC, /*!< Function is not supported on attached target */
    ESP_LOADER_ERROR_INVALID_RESPONSE  /*!< Internal error */
} esp_loader_error_t;

#ifdef __cplusplus
}
#endif
