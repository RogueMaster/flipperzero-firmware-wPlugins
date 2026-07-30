/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_loader_io.h"

/* Select the STM32 HAL header by probing which family header is reachable on
 * the current include path.  This works both with the community stm32-cmake
 * toolchain (which defines a family-level macro such as STM32H7) and with the
 * STM32CubeMX CMake generator (which only defines a device-level macro such as
 * STM32H743xx).  __has_include is a GCC/Clang extension supported by all
 * arm-none-eabi-gcc versions in common use. */
#if   __has_include("stm32c0xx_hal.h")
#  include "stm32c0xx_hal.h"
#elif __has_include("stm32f0xx_hal.h")
#  include "stm32f0xx_hal.h"
#elif __has_include("stm32f1xx_hal.h")
#  include "stm32f1xx_hal.h"
#elif __has_include("stm32f2xx_hal.h")
#  include "stm32f2xx_hal.h"
#elif __has_include("stm32f3xx_hal.h")
#  include "stm32f3xx_hal.h"
#elif __has_include("stm32f4xx_hal.h")
#  include "stm32f4xx_hal.h"
#elif __has_include("stm32f7xx_hal.h")
#  include "stm32f7xx_hal.h"
#elif __has_include("stm32g0xx_hal.h")
#  include "stm32g0xx_hal.h"
#elif __has_include("stm32g4xx_hal.h")
#  include "stm32g4xx_hal.h"
#elif __has_include("stm32h5xx_hal.h")
#  include "stm32h5xx_hal.h"
#elif __has_include("stm32h7xx_hal.h")
#  include "stm32h7xx_hal.h"
#elif __has_include("stm32l0xx_hal.h")
#  include "stm32l0xx_hal.h"
#elif __has_include("stm32l1xx_hal.h")
#  include "stm32l1xx_hal.h"
#elif __has_include("stm32l4xx_hal.h")
#  include "stm32l4xx_hal.h"
#elif __has_include("stm32l5xx_hal.h")
#  include "stm32l5xx_hal.h"
#elif __has_include("stm32u0xx_hal.h")
#  include "stm32u0xx_hal.h"
#elif __has_include("stm32u5xx_hal.h")
#  include "stm32u5xx_hal.h"
#elif __has_include("stm32wbxx_hal.h")
#  include "stm32wbxx_hal.h"
#elif __has_include("stm32wlxx_hal.h")
#  include "stm32wlxx_hal.h"
#else
#  error "No STM32 HAL header found in the include path. " \
"Ensure the STM32 HAL Drivers include directory is passed to the compiler."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Concrete STM32 UART port instance.
 *
 * Declare one of these, fill the config fields, then pass &port.port to
 * esp_loader_init_serial(). Hardware initialisation is called automatically
 * inside esp_loader_init_serial() — no separate init step is needed.
 *
 * @code
 *   stm32_port_t port = {
 *       .port.ops    = &stm32_uart_ops,
 *       .huart       = &huart2,
 *       .port_boot    = TARGET_BOOT_GPIO_Port,
 *       .pin_num_boot = TARGET_BOOT_Pin,
 *       .port_rst    = TARGET_RESET_GPIO_Port,
 *       .pin_num_rst = TARGET_RESET_Pin,
 *   };
 *   esp_loader_t loader;
 *   esp_loader_init_serial(&loader, &port.port);
 * @endcode
 */
typedef struct {
    esp_loader_port_t   port;         /*!< Embedded port base */

    /* Configuration — fill before calling esp_loader_init_serial() */
    UART_HandleTypeDef *huart;
    GPIO_TypeDef       *port_boot;
    uint16_t            pin_num_boot;
    GPIO_TypeDef       *port_rst;
    uint16_t            pin_num_rst;

    /* Private runtime state — do not access directly */
    uint32_t _time_end;
} stm32_port_t;

/** Port operations vtable for the STM32 UART port. */
extern const esp_loader_port_ops_t stm32_uart_ops;

#ifdef __cplusplus
}
#endif
