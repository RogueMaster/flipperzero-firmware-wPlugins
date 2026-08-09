/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h>
#include <stdio.h>
#include "stm32_port.h"
#include "loader_port_stdio_log.h"

static esp_loader_error_t stm32_port_init(esp_loader_port_t *port)
{
    (void)port;
    /* UART and GPIO peripherals are already initialised by STM32 HAL CubeMX code
     * before esp_loader_init_serial() is called.  Nothing to do here. */
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t stm32_uart_write(esp_loader_port_t *port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    HAL_StatusTypeDef err = HAL_UART_Transmit(p->huart, (uint8_t *)data, size, timeout);

    if (err == HAL_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == HAL_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static esp_loader_error_t stm32_uart_read(esp_loader_port_t *port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    HAL_StatusTypeDef err = HAL_UART_Receive(p->huart, data, size, timeout);

    if (err == HAL_OK) {
        return ESP_LOADER_SUCCESS;
    } else if (err == HAL_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}

static void stm32_uart_delay_ms(esp_loader_port_t *port, uint32_t ms)
{
    (void)port;
    HAL_Delay(ms);
}

static void stm32_uart_start_timer(esp_loader_port_t *port, uint32_t ms)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    p->_time_end = HAL_GetTick() + ms;
}

static uint32_t stm32_uart_remaining_time(esp_loader_port_t *port)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    int32_t remaining = (int32_t)(p->_time_end - HAL_GetTick());
    return (remaining > 0) ? (uint32_t)remaining : 0;
}

static void stm32_uart_reset_target(esp_loader_port_t *port)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    HAL_GPIO_WritePin(p->port_rst, p->pin_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    HAL_GPIO_WritePin(p->port_rst, p->pin_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void stm32_uart_enter_bootloader(esp_loader_port_t *port)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    HAL_GPIO_WritePin(p->port_boot, p->pin_num_boot, SERIAL_FLASHER_BOOT_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(p->port_rst, p->pin_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_Delay(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    HAL_GPIO_WritePin(p->port_rst, p->pin_num_rst, SERIAL_FLASHER_RESET_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_Delay(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    HAL_GPIO_WritePin(p->port_boot, p->pin_num_boot, SERIAL_FLASHER_BOOT_INVERT ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static esp_loader_error_t stm32_uart_change_rate(esp_loader_port_t *port, uint32_t baudrate)
{
    stm32_port_t *p = container_of(port, stm32_port_t, port);
    HAL_UART_DeInit(p->huart);
    p->huart->Init.BaudRate = baudrate;

    if (HAL_UART_Init(p->huart) != HAL_OK) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

const esp_loader_port_ops_t stm32_uart_ops = {
    .init                     = stm32_port_init,
    .deinit                   = NULL,
    .enter_bootloader         = stm32_uart_enter_bootloader,
    .reset_target             = stm32_uart_reset_target,
    .start_timer              = stm32_uart_start_timer,
    .remaining_time           = stm32_uart_remaining_time,
    .delay_ms                 = stm32_uart_delay_ms,
    .log                      = loader_port_stdio_log,
    .log_hex                  = loader_port_stdio_log_hex,
    .change_transmission_rate = stm32_uart_change_rate,
    .write                    = stm32_uart_write,
    .read                     = stm32_uart_read,
};
