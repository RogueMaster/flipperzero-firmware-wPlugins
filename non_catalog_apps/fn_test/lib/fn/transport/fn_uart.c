#include "fn_uart_i.h"

#include "core/fn_frame.h"
#include "core/fn_wire.h"

#include <string.h>

#define TAG "FNUart"

static FuriHalSerialId fn_uart_serial_id(void) {
#ifdef FURI_DEBUG
    return FuriHalSerialIdLpuart;
#else
    return FuriHalSerialIdUsart;
#endif
}

uint32_t fn_uart_default_timeout(void) {
    return FN_UART_BAUD_TIMEOUT_MS;
}

/**
 * DMA-RX колбэк: выполняется в прерывании.
 * Здесь допустимы только `furi_hal_serial_dma_rx()` и
 * `furi_stream_buffer_send()` — они ISR-safe и сами будят ожидающий поток.
 */
static void fn_uart_rx_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    size_t size,
    void* context) {
    FnUart* uart = context;
    if(!uart) return;

    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        while(size > 0) {
            const size_t chunk = (size > sizeof(uart->dma_buf)) ? sizeof(uart->dma_buf) : size;
            const size_t got = furi_hal_serial_dma_rx(handle, uart->dma_buf, chunk);
            if(got == 0) break;
            const size_t sent = furi_stream_buffer_send(uart->rx_stream, uart->dma_buf, got, 0);
            if(sent != got) {
                uart->overflow = true;
            }
            size -= got;
        }
    }
}

FnUart* fn_uart_alloc(void) {
    FnUart* uart = malloc(sizeof(FnUart));
    if(!uart) return NULL;
    memset(uart, 0, sizeof(FnUart));

    uart->expansion = furi_record_open(RECORD_EXPANSION);
    /* Служба expansion по умолчанию держит USART (пины 13/14). */
    expansion_disable(uart->expansion);

    uart->serial = furi_hal_serial_control_acquire(fn_uart_serial_id());
    if(!uart->serial) {
        FURI_LOG_E(TAG, "порт занят: захват USART не удался");
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        free(uart);
        return NULL;
    }
    furi_hal_serial_init(uart->serial, FN_UART_BAUDRATE);

    uart->rx_stream = furi_stream_buffer_alloc(FN_FRAME_MAX_LEN, 1);
    furi_hal_serial_dma_rx_start(uart->serial, fn_uart_rx_cb, uart, true);
    uart->rx_ready = true;
    return uart;
}

void fn_uart_free(FnUart* uart) {
    if(!uart) return;

    uart->rx_ready = false;
    if(uart->serial) {
        furi_hal_serial_dma_rx_stop(uart->serial);
        furi_hal_serial_deinit(uart->serial);
        furi_hal_serial_control_release(uart->serial);
        uart->serial = NULL;
    }
    if(uart->rx_stream) {
        furi_stream_buffer_free(uart->rx_stream);
        uart->rx_stream = NULL;
    }
    if(uart->expansion) {
        expansion_enable(uart->expansion);
        furi_record_close(RECORD_EXPANSION);
        uart->expansion = NULL;
    }
    free(uart);
}

bool fn_uart_is_busy(const FnUart* uart) {
    return uart && uart->busy;
}

/** Проверка: в буфере лежит целый кадр ответа. */
static bool fn_uart_frame_complete(const uint8_t* rx, size_t rx_len) {
    if(rx_len >= 3) {
        const uint16_t wire_len = fn_wire_u16le_get(&rx[1]);
        const size_t expected = (size_t)wire_len + 5;
        if(rx_len >= expected) return true;
    }
    return rx_len >= FN_FRAME_MAX_LEN;
}

FnUartTrxStatus fn_uart_trx(
    FnUart* uart,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t timeout_ms) {
    if(rx_len) *rx_len = 0;
    if(!uart || !tx || tx_len == 0 || !rx || !rx_len || rx_cap == 0) {
        return FnUartTrxError;
    }
    if(uart->busy) return FnUartTrxError;
    if(!uart->rx_ready || !uart->serial || !uart->rx_stream) return FnUartTrxError;

#ifdef FN_TESTS
    if(uart->inject_len != 0) {
        const size_t inject_len = uart->inject_len;
        const size_t copy = (inject_len > rx_cap) ? rx_cap : inject_len;
        memcpy(rx, uart->inject_buf, copy);
        *rx_len = copy;
        uart->inject_len = 0;
        return FnUartTrxOk;
    }
#endif

    uart->busy = true;
    uart->cancel_requested = false;
    uart->overflow = false;
    furi_stream_buffer_reset(uart->rx_stream);

    if(timeout_ms == 0) timeout_ms = FN_UART_BAUD_TIMEOUT_MS;
    const uint32_t started = furi_get_tick();
    const uint32_t timeout_ticks = furi_ms_to_ticks(timeout_ms);

    furi_hal_serial_tx(uart->serial, tx, tx_len);

    FnUartTrxStatus status = FnUartTrxTimeout;
    size_t total = 0;

    for(;;) {
        if(uart->cancel_requested) {
            status = FnUartTrxCancelled;
            break;
        }

        if(total < rx_cap) {
            /* Блокирующее ожидание до SLICE_MS: поток будится записью из ISR. */
            total += furi_stream_buffer_receive(
                uart->rx_stream, &rx[total], rx_cap - total, FN_UART_SLICE_MS);
        }

        if(fn_uart_frame_complete(rx, total)) {
            status = FnUartTrxOk;
            break;
        }
        if(total >= rx_cap) {
            /* Кадр не помещается в буфер: пусть вызывающий увидит данные
             * и разберёт их (получит TooLong/LengthMismatch). */
            status = FnUartTrxOk;
            break;
        }
        if(furi_get_tick() - started >= timeout_ticks) {
            status = (total >= FN_FRAME_OVERHEAD) ? FnUartTrxOk : FnUartTrxTimeout;
            break;
        }
    }

    if(uart->overflow) {
        FURI_LOG_W(TAG, "переполнение буфера приема");
    }

    *rx_len = total;
    furi_stream_buffer_reset(uart->rx_stream);
    uart->busy = false;
    uart->cancel_requested = false;
    return status;
}

void fn_uart_cancel(FnUart* uart) {
    if(!uart || !uart->busy) return;
    /* Ожидание идёт короткими слайсами: флаг проверяется не позже SLICE_MS. */
    uart->cancel_requested = true;
}

#ifdef FN_TESTS
void fn_uart_test_inject(FnUart* uart, const uint8_t* data, size_t len) {
    if(!uart) return;
    if(!data || len == 0 || len > sizeof(uart->inject_buf)) {
        uart->inject_len = 0;
        return;
    }
    memcpy(uart->inject_buf, data, len);
    uart->inject_len = len;
}
#endif
