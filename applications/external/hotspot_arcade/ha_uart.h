#pragma once

#include <furi.h>
#include <furi_hal_serial.h>

typedef struct HaUart HaUart;

// Called from the UART worker thread when RX bytes are available. It must only
// signal (post a ViewDispatcher custom event); bytes are drained on the consumer
// (GUI) thread via ha_uart_rx(). The context is fixed at init.
typedef void (*HaUartNotify)(void* ctx);

HaUart* ha_uart_init(uint32_t baudrate, HaUartNotify notify, void* notify_ctx);

// Two-phase teardown: stop the RX worker before the ViewDispatcher is freed, then
// still tx a final command, then release.
void ha_uart_stop_rx(HaUart* uart);
void ha_uart_deinit(HaUart* uart);

// Drain up to maxlen RX bytes (single consumer thread). Returns bytes copied.
size_t ha_uart_rx(HaUart* uart, uint8_t* buf, size_t maxlen);

void ha_uart_tx(HaUart* uart, const uint8_t* data, size_t len);

// Lend the serial line to a raw user (the ESP flasher). `suspend` stops our async
// RX so nothing else consumes bytes and hands back the underlying handle; `resume`
// restores our RX at the normal session baud. Suspend before flashing, resume after.
FuriHalSerialHandle* ha_uart_serial(HaUart* uart);
void ha_uart_suspend(HaUart* uart);
void ha_uart_resume(HaUart* uart);
