#pragma once
#include <furi_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include "yrm100x_buffer.h"

/**
 * File that handles the uart for the YRM100
 * @author frux-c
*/

#define UHF_UART_RX_BUFFER_SIZE   500
#define UHF_UART_DEFAULT_BAUDRATE 115200
#define UHF_UART_FRAME_START      0xBB
#define UHF_UART_FRAME_END        0x7E
// First-byte timeout for a command response, expressed in busy-spin iterations.
// The ISR resets this counter on every received byte, so it effectively bounds how
// long we wait for the module to START replying. At the edge of read range the
// module runs internal Gen2 retries and can take a long time before its first byte.
// Logs showed the module's worst-case first-byte latency landing right around a
// 40000-tick timeout: the reply would arrive just after we gave up, corrupt the
// *next* command's window, and get discarded as a stale frame (frame desync) —
// throwing away the real answer and forcing a full retry. Sizing this comfortably
// above that worst-case latency keeps each reply inside its own window, which
// eliminates most of the desync churn and the wasted retry cycles.
#define UHF_UART_WAIT_TICK        70000

typedef void (*CallbackFunction)(uint8_t* data, void* ctx);

typedef enum {
    UHFUartWorkerWaitingDataFlag = 1 << 0,
    UHFUartWorkerExitingFlag = 1 << 2,
} UHFUartWorkerEventFlag;

typedef struct {
    FuriHalBus bus;
    FuriHalSerialHandle* handle;
    // FuriStreamBuffer *rx_buff_stream;
    // FuriThread *thread;
    CallbackFunction callback;
    Buffer* buffer;
    uint32_t baudrate;
    bool init_by_app;
    void* ctx;
    volatile int tick;
} UHFUart;

int32_t uhf_uart_worker_callback(void* ctx);

UHFUart* uhf_uart_alloc();
void uhf_uart_free(UHFUart* uart);
void uhf_uart_send(UHFUart* uart, uint8_t* data, size_t size);
void uhf_uart_send_wait(UHFUart* uart, uint8_t* data, size_t size);
void uhf_uart_set_receive_byte_callback(
    UHFUart* uart,
    FuriHalSerialAsyncRxCallback callback,
    void* ctx,
    bool report_errors);
void uhf_uart_set_baudrate(UHFUart* uart, uint32_t baudrate);
bool uhf_uart_tick(UHFUart* uart);
void uhf_uart_tick_reset(UHFUart* uart);
