#pragma once

#include "uart_i.h"

int32_t seos_uart_tx_thread(void* context);
void seos_uart_on_irq_cb(uint8_t data, void* context);
void seos_uart_serial_init(SeosUart* seos_uart, uint8_t uart_ch);
void seos_uart_serial_deinit(SeosUart* seos_uart);
void seos_uart_set_baudrate(SeosUart* seos_uart, uint32_t baudrate);
int32_t seos_uart_worker(void* context);

SeosUart* seos_uart_enable(SeosUartConfig* cfg);
void seos_uart_disable(SeosUart* seos_uart);
void seos_uart_set_config(SeosUart* seos_uart, SeosUartConfig* cfg);
void seos_uart_get_config(SeosUart* seos_uart, SeosUartConfig* cfg);

SeosUart* seos_uart_alloc();
void seos_uart_free(SeosUart* seos_uart);

void seos_uart_send(SeosUart* seos_uart, uint8_t* buffer, size_t len);
void seos_uart_set_receive_callback(
    SeosUart* seos_uart,
    SeosUartReceiveCallback callback,
    void* context);
