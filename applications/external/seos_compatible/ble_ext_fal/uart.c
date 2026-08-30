#include "seos_i.h"
#include "uart.h"

#define TAG "SeosUART"

#define SEOS_UART_BAUD 460800

static void seos_uart_on_irq_rx_dma_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    size_t size,
    void* context) {
    SeosUart* seos_uart = (SeosUart*)context;
    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE] = {0};
        while(size) {
            size_t ret = furi_hal_serial_dma_rx(
                handle,
                data,
                (size > FURI_HAL_SERIAL_DMA_BUFFER_SIZE) ? FURI_HAL_SERIAL_DMA_BUFFER_SIZE : size);
            furi_stream_buffer_send(seos_uart->rx_stream, data, ret, 0);
            size -= ret;
        };
        furi_thread_flags_set(furi_thread_get_id(seos_uart->thread), WorkerEvtRxDone);
    }
}

void seos_uart_disable(SeosUart* seos_uart) {
    furi_assert(seos_uart);
    furi_thread_flags_set(furi_thread_get_id(seos_uart->thread), WorkerEvtStop);
    furi_thread_join(seos_uart->thread);
    furi_thread_free(seos_uart->thread);
    free(seos_uart);
}

void seos_uart_serial_init(SeosUart* seos_uart, uint8_t uart_ch) {
    furi_assert(!seos_uart->serial_handle);
    SeosUartConfig cfg = seos_uart->cfg;

    seos_uart->serial_handle = furi_hal_serial_control_acquire(uart_ch);
    furi_assert(seos_uart->serial_handle);

    furi_hal_serial_init(seos_uart->serial_handle, cfg.baudrate);
    furi_hal_serial_dma_rx_start(
        seos_uart->serial_handle, seos_uart_on_irq_rx_dma_cb, seos_uart, false);
}

void seos_uart_serial_deinit(SeosUart* seos_uart) {
    furi_assert(seos_uart->serial_handle);
    furi_hal_serial_deinit(seos_uart->serial_handle);
    furi_hal_serial_control_release(seos_uart->serial_handle);
    seos_uart->serial_handle = NULL;
}

void seos_uart_set_baudrate(SeosUart* seos_uart, uint32_t baudrate) {
    if(baudrate != 0) {
        furi_hal_serial_set_br(seos_uart->serial_handle, baudrate);
    } else {
        FURI_LOG_I(TAG, "No baudrate specified");
    }
}

size_t seos_uart_process_buffer(SeosUart* seos_uart, uint8_t* cmd, size_t cmd_len) {
    if(cmd_len < 2) {
        return cmd_len;
    }

    size_t consumed = 0;
    do {
        if(seos_uart->receive_callback) {
            consumed =
                seos_uart->receive_callback(seos_uart->receive_callback_context, cmd, cmd_len);
        }

        if(consumed > 0) {
            memset(cmd, 0, consumed);
            cmd_len -= consumed;
            if(cmd_len > 0) {
                memmove(cmd, cmd + consumed, cmd_len);
            }

            /*
            memset(display, 0, SEOS_UART_RX_BUF_SIZE);
            for(uint8_t i = 0; i < cmd_len; i++) {
                snprintf(display + (i * 2), sizeof(display), "%02x", cmd[i]);
            }
            FURI_LOG_I(TAG, "cmd is now %d bytes: %s", cmd_len, display);
            */
        }
    } while(consumed > 0 && cmd_len > 0);
    return cmd_len;
}

int32_t seos_uart_worker(void* context) {
    SeosUart* seos_uart = (SeosUart*)context;
    furi_thread_set_current_priority(FuriThreadPriorityHighest);

    memcpy(&seos_uart->cfg, &seos_uart->cfg_new, sizeof(SeosUartConfig));

    seos_uart->rx_stream = furi_stream_buffer_alloc(SEOS_UART_RX_BUF_SIZE, 1);

    seos_uart_serial_init(seos_uart, seos_uart->cfg.uart_ch);
    seos_uart_set_baudrate(seos_uart, seos_uart->cfg.baudrate);

    uint8_t cmd[SEOS_UART_RX_BUF_SIZE];
    size_t cmd_len = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        furi_check(!(events & FuriFlagError));
        if(events & WorkerEvtStop) {
            memset(cmd, 0, cmd_len);
            cmd_len = 0;
            break;
        }
        if(events & (WorkerEvtRxDone | WorkerEvtDevTxComplete)) {
            size_t len = furi_stream_buffer_receive(
                seos_uart->rx_stream, seos_uart->rx_buf, SEOS_UART_RX_BUF_SIZE, 0);
            if(len > 0) {
                furi_delay_ms(5); //WTF

                /*
                char display[SEOS_UART_RX_BUF_SIZE * 2 + 1] = {0};
                for(uint8_t i = 0; i < len; i++) {
                    snprintf(display + (i * 2), sizeof(display), "%02x", seos_uart->rx_buf[i]);
                }
                FURI_LOG_D(TAG, "RECV %d bytes: %s", len, display);
                */

                if(cmd_len + len > SEOS_UART_RX_BUF_SIZE) {
                    FURI_LOG_I(TAG, "OVERFLOW: %d + %d", cmd_len, len);
                    memset(cmd, 0, cmd_len);
                    cmd_len = 0;
                }

                memcpy(cmd + cmd_len, seos_uart->rx_buf, len);
                cmd_len += len;
                cmd_len = seos_uart_process_buffer(seos_uart, cmd, cmd_len);
            }
        }
    }
    seos_uart_serial_deinit(seos_uart);

    furi_stream_buffer_free(seos_uart->rx_stream);
    return 0;
}

SeosUart* seos_uart_enable(SeosUartConfig* cfg) {
    SeosUart* seos_uart = malloc(sizeof(SeosUart));

    memcpy(&(seos_uart->cfg_new), cfg, sizeof(SeosUartConfig));

    seos_uart->thread =
        furi_thread_alloc_ex("SeosUartWorker", 5 * 1024, seos_uart_worker, seos_uart);

    furi_thread_start(seos_uart->thread);
    return seos_uart;
}

void seos_uart_get_config(SeosUart* seos_uart, SeosUartConfig* cfg) {
    furi_assert(seos_uart);
    furi_assert(cfg);
    memcpy(cfg, &(seos_uart->cfg_new), sizeof(SeosUartConfig));
}

SeosUart* seos_uart_alloc() {
    SeosUartConfig cfg = {.uart_ch = FuriHalSerialIdLpuart, .baudrate = SEOS_UART_BAUD};
    SeosUart* seos_uart;

    FURI_LOG_I(TAG, "Enable UART");
    seos_uart = seos_uart_enable(&cfg);

    seos_uart_get_config(seos_uart, &cfg);
    return seos_uart;
}

void seos_uart_free(SeosUart* seos_uart) {
    seos_uart_disable(seos_uart);
}

void seos_uart_send(SeosUart* seos_uart, uint8_t* buffer, size_t len) {
    /* furi_hal_serial_tx waits on the transfer itself, so there is nothing for
     * a thread of our own to do. Sending straight from the caller's buffer
     * also removes the copy that a second thread could read while this one was
     * still filling it. */
    if(!seos_uart->serial_handle) {
        FURI_LOG_W(TAG, "Send before the port was ready");
        return;
    }
    furi_hal_serial_tx(seos_uart->serial_handle, buffer, len);
}

void seos_uart_set_receive_callback(
    SeosUart* seos_uart,
    SeosUartReceiveCallback callback,
    void* context) {
    seos_uart->receive_callback = callback;
    seos_uart->receive_callback_context = context;
}
