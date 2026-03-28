#include "seader_i.h"
#include "trace_log.h"

#define TAG                              "SeaderUART"
#define BAUDRATE_DEFAULT                 115200
#define SEADER_UART_WORKER_STACK_SIZE    (3U * 1024U)
#define SEADER_UART_TX_WORKER_STACK_SIZE (1024U)

static void seader_uart_on_irq_rx_dma_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    size_t size,
    void* context) {
    SeaderUartBridge* seader_uart = (SeaderUartBridge*)context;
    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE] = {0};
        while(size) {
            size_t ret = furi_hal_serial_dma_rx(
                handle,
                data,
                (size > FURI_HAL_SERIAL_DMA_BUFFER_SIZE) ? FURI_HAL_SERIAL_DMA_BUFFER_SIZE : size);
            furi_stream_buffer_send(seader_uart->rx_stream, data, ret, 0);
            size -= ret;
        };
        furi_thread_flags_set(furi_thread_get_id(seader_uart->thread), WorkerEvtRxDone);
    }
}

void seader_uart_disable(SeaderUartBridge* seader_uart) {
    furi_assert(seader_uart);
    furi_thread_flags_set(furi_thread_get_id(seader_uart->thread), WorkerEvtStop);
    furi_thread_join(seader_uart->thread);
    furi_thread_free(seader_uart->thread);
    free(seader_uart);
}

void seader_uart_serial_init(SeaderUartBridge* seader_uart, uint8_t uart_ch) {
    furi_assert(!seader_uart->serial_handle);

    seader_uart->serial_handle = furi_hal_serial_control_acquire(uart_ch);
    furi_assert(seader_uart->serial_handle);

    furi_hal_serial_init(seader_uart->serial_handle, BAUDRATE_DEFAULT);
    furi_hal_serial_dma_rx_start(
        seader_uart->serial_handle, seader_uart_on_irq_rx_dma_cb, seader_uart, false);
}

void seader_uart_serial_deinit(SeaderUartBridge* seader_uart) {
    furi_assert(seader_uart->serial_handle);
    furi_hal_serial_deinit(seader_uart->serial_handle);
    furi_hal_serial_control_release(seader_uart->serial_handle);
    seader_uart->serial_handle = NULL;
}

size_t seader_uart_process_buffer(Seader* seader, uint8_t* cmd, size_t cmd_len) {
    if(cmd_len < 2) {
        return cmd_len;
    }

    size_t consumed = 0;
    do {
        consumed = seader_ccid_process(seader, cmd, cmd_len);

        if(consumed > 0) {
            memset(cmd, 0, consumed);
            cmd_len -= consumed;
            if(cmd_len > 0) {
                memmove(cmd, cmd + consumed, cmd_len);
            }

            /*
            memset(display, 0, SEADER_UART_RX_BUF_SIZE);
            for (uint8_t i = 0; i < cmd_len; i++) {
                snprintf(display+(i*2), sizeof(display), "%02x", cmd[i]);
            }
            FURI_LOG_I(TAG, "cmd is now %d bytes: %s", cmd_len, display);
            */
        }
    } while(consumed > 0 && cmd_len > 0);
    return cmd_len;
}

int32_t seader_uart_worker(void* context) {
    Seader* seader = (Seader*)context;
    SeaderUartBridge* seader_uart = seader->uart;
    furi_thread_set_current_priority(FuriThreadPriorityHighest);

    memcpy(&seader_uart->cfg, &seader_uart->cfg_new, sizeof(SeaderUartConfig));

    seader_uart->rx_stream = furi_stream_buffer_alloc(SEADER_UART_RX_BUF_SIZE, 1);

    seader_uart->tx_sem = furi_semaphore_alloc(1, 1);

    seader_uart->tx_thread = furi_thread_alloc_ex(
        "SeaderUartTxWorker", SEADER_UART_TX_WORKER_STACK_SIZE, seader_uart_tx_thread, seader);

    seader_uart_serial_init(seader_uart, seader_uart->cfg.uart_ch);
    furi_hal_serial_set_br(seader_uart->serial_handle, seader_uart->cfg.baudrate);

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtSamRx);

    furi_thread_start(seader_uart->tx_thread);

    uint8_t cmd[SEADER_UART_RX_BUF_SIZE];
    size_t cmd_len = 0;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_RX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        if(events & FuriFlagError) {
            FURI_LOG_E(
                TAG,
                "RX worker flag error events=0x%08lx thread=%p tx_thread=%p",
                (unsigned long)events,
                (void*)seader_uart->thread,
                (void*)seader_uart->tx_thread);
            break;
        }
        if(events & WorkerEvtStop) {
            memset(cmd, 0, cmd_len);
            cmd_len = 0;
            break;
        }
        if(events & (WorkerEvtRxDone | WorkerEvtSamTxComplete)) {
            if(cmd_len >= sizeof(cmd)) {
                FURI_LOG_I(TAG, "RX buffer full, resetting");
                memset(cmd, 0, sizeof(cmd));
                cmd_len = 0;
            }

            size_t len = furi_stream_buffer_receive(
                seader_uart->rx_stream, cmd + cmd_len, sizeof(cmd) - cmd_len, 0);
            if(len > 0) {
                furi_delay_ms(5); //WTF

                /*
                char display[SEADER_UART_RX_BUF_SIZE * 2 + 1] = {0};
                for (uint8_t i = 0; i < len; i++) {
                    snprintf(display+(i*2), sizeof(display), "%02x", cmd[cmd_len + i]);
                }
                FURI_LOG_I(TAG, "RECV %d bytes: %s", len, display);
                */
                cmd_len += len;
                cmd_len = seader_uart_process_buffer(seader, cmd, cmd_len);
            }
        }
    }
    seader_uart_serial_deinit(seader_uart);

    furi_thread_flags_set(furi_thread_get_id(seader_uart->tx_thread), WorkerEvtTxStop);
    furi_thread_join(seader_uart->tx_thread);
    furi_thread_free(seader_uart->tx_thread);

    furi_stream_buffer_free(seader_uart->rx_stream);
    furi_semaphore_free(seader_uart->tx_sem);
    return 0;
}

SeaderUartBridge* seader_uart_enable(SeaderUartConfig* cfg, Seader* seader) {
    SeaderUartBridge* seader_uart = calloc(1, sizeof(SeaderUartBridge));

    seader_uart->T = 1;
    seader_t_1_reset(seader_uart);
    seader_uart->ccid.retries = 3;

    memcpy(&(seader_uart->cfg_new), cfg, sizeof(SeaderUartConfig));

    seader_uart->thread = furi_thread_alloc_ex(
        "SeaderUartWorker", SEADER_UART_WORKER_STACK_SIZE, seader_uart_worker, seader);

    furi_thread_start(seader_uart->thread);
    return seader_uart;
}

int32_t seader_uart_tx_thread(void* context) {
    Seader* seader = (Seader*)context;
    SeaderUartBridge* seader_uart = seader->uart;

    furi_thread_set_current_priority(FuriThreadPriorityHighest);
    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_TX_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        if(events & FuriFlagError) {
            FURI_LOG_E(
                TAG,
                "TX worker flag error events=0x%08lx serial_handle=%p",
                (unsigned long)events,
                (void*)seader_uart->serial_handle);
            break;
        }
        if(events & WorkerEvtTxStop) break;
        if(events & WorkerEvtSamRx) {
            if(seader_uart->tx_len > 0) {
                furi_hal_serial_tx(
                    seader_uart->serial_handle, seader_uart->tx_buf, seader_uart->tx_len);
            }
        }
    }
    return 0;
}

void seader_uart_get_config(SeaderUartBridge* seader_uart, SeaderUartConfig* cfg) {
    furi_assert(seader_uart);
    furi_assert(cfg);
    memcpy(cfg, &(seader_uart->cfg_new), sizeof(SeaderUartConfig));
}

void seader_uart_get_state(SeaderUartBridge* seader_uart, SeaderUartState* st) {
    furi_assert(seader_uart);
    furi_assert(st);
    memcpy(st, &(seader_uart->st), sizeof(SeaderUartState));
}

SeaderUartBridge* seader_uart_alloc(Seader* seader) {
    SeaderUartConfig cfg = {.uart_ch = FuriHalSerialIdLpuart, .baudrate = BAUDRATE_DEFAULT};
    SeaderUartState uart_state;
    SeaderUartBridge* seader_uart;

    SEADER_VERBOSE_I(TAG, "Enable UART");
    seader_uart = seader_uart_enable(&cfg, seader);

    seader_uart_get_config(seader_uart, &cfg);
    seader_uart_get_state(seader_uart, &uart_state);
    return seader_uart;
}

void seader_uart_free(SeaderUartBridge* seader_uart) {
    seader_uart_disable(seader_uart);
}
