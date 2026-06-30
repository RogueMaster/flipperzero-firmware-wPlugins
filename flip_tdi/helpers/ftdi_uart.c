#include "ftdi_uart.h"
#include "furi.h"
#include <furi_hal.h>

#include <stm32wbxx_ll_lpuart.h>
#include <stm32wbxx_ll_dma.h>

#define TAG "FTDI_UART"
#define FTDI_UART_MAX_TXRX_SIZE (256UL)

#define FTDI_UART_LPUART_DMA_INSTANCE (DMA2)
#define FTDI_UART_LPUART_DMA_CHANNEL (LL_DMA_CHANNEL_1)

struct FtdiUart {
    FuriThread* worker_thread;
    FuriHalSerialHandle* serial_handle;
    uint32_t baudrate;
    Ftdi* ftdi;
    uint8_t* buffer_tx_ptr;
    bool enable;
};

typedef enum {
    WorkerEventReserved = (1 << 0), // Reserved for StreamBuffer internal event
    WorkerEventStop = (1 << 1),
    WorkerEventRxData = (1 << 2),
    WorkerEventRxIdle = (1 << 3),
    WorkerEventRxOverrunError = (1 << 4),
    WorkerEventRxFramingError = (1 << 5),
    WorkerEventRxNoiseError = (1 << 6),
    WorkerEventTXData = (1 << 7),
    WorkerEventTXDataDmaEnd = (1 << 8),
} WorkerEvent;

#define WORKER_EVENTS_MASK                                                                 \
    (WorkerEventStop | WorkerEventRxData | WorkerEventRxIdle | WorkerEventRxOverrunError | \
     WorkerEventRxFramingError | WorkerEventRxNoiseError | WorkerEventTXData |             \
     WorkerEventTXDataDmaEnd)

static void ftdi_uart_tx_dma_init(FtdiUart* ftdi_uart);
static void ftdi_uart_tx_dma_deinit(FtdiUart* ftdi_uart);
static void ftdi_uart_tx_dma(FtdiUart* ftdi_uart, uint8_t* data, size_t size);

static void ftdi_uart_irq_cb(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent ev,
    size_t size,
    void* context) {
    FtdiUart* ftdi_uart = context;

    if(ev & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[FURI_HAL_SERIAL_DMA_BUFFER_SIZE] = {0};
        while(size) {
            size_t ret = furi_hal_serial_dma_rx(
                handle,
                data,
                (size > FURI_HAL_SERIAL_DMA_BUFFER_SIZE) ? FURI_HAL_SERIAL_DMA_BUFFER_SIZE : size);
            ftdi_set_tx_buf(ftdi_uart->ftdi, data, ret);
            size -= ret;
        };
    }
}

static int32_t ftdi_uart_echo_worker(void* context) {
    furi_assert(context);
    FtdiUart* ftdi_uart = context;

    FURI_LOG_I(TAG, "Worker started");

    ftdi_uart_tx_dma_init(ftdi_uart);
    bool is_dma_tx = false;

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_EVENTS_MASK, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEventStop) break;

        if(events & WorkerEventTXData) {
            if(!is_dma_tx) {
                is_dma_tx = true;
                events |= WorkerEventTXDataDmaEnd;
            }
        }

        if(events & WorkerEventTXDataDmaEnd) {
            size_t length = 0;
            length = ftdi_get_rx_buf(
                ftdi_uart->ftdi, ftdi_uart->buffer_tx_ptr, FTDI_UART_MAX_TXRX_SIZE, 0);
            if(length > 0) {
                ftdi_uart_tx_dma(ftdi_uart, ftdi_uart->buffer_tx_ptr, length);
            } else {
                is_dma_tx = false;
            }
        }
    }
    ftdi_uart_tx_dma_deinit(ftdi_uart);
    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

FtdiUart* ftdi_uart_alloc(Ftdi* ftdi) {
    FtdiUart* ftdi_uart = malloc(sizeof(FtdiUart));
    ftdi_uart->baudrate = 115200;

    ftdi_uart->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
    furi_check(ftdi_uart->serial_handle);
    furi_hal_serial_init(ftdi_uart->serial_handle, ftdi_uart->baudrate);
    furi_hal_serial_enable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionRx);
    furi_hal_serial_enable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionTx);
    furi_hal_serial_dma_rx_start(ftdi_uart->serial_handle, ftdi_uart_irq_cb, ftdi_uart, false);
    ftdi_uart->ftdi = ftdi;

    //do not change LPUART, functions that directly work with peripherals are used
    ftdi_uart->worker_thread = furi_thread_alloc_ex(TAG, 1024, ftdi_uart_echo_worker, ftdi_uart);
    ftdi_uart->enable = true;

    furi_thread_start(ftdi_uart->worker_thread);

    return ftdi_uart;
}

void ftdi_uart_free(FtdiUart* ftdi_uart) {
    ftdi_uart->enable = false;
    furi_thread_flags_set(furi_thread_get_id(ftdi_uart->worker_thread), WorkerEventStop);
    furi_thread_join(ftdi_uart->worker_thread);
    furi_thread_free(ftdi_uart->worker_thread);

    furi_hal_serial_deinit(ftdi_uart->serial_handle);
    furi_hal_serial_control_release(ftdi_uart->serial_handle);
    ftdi_uart->serial_handle = NULL;
    free(ftdi_uart);
}

void ftdi_uart_tx(FtdiUart* ftdi_uart) {
    if(ftdi_uart->enable) {
        furi_thread_flags_set(furi_thread_get_id(ftdi_uart->worker_thread), WorkerEventTXData);
    }
}

void ftdi_uart_set_baudrate(FtdiUart* ftdi_uart, uint32_t baudrate) {
    ftdi_uart->baudrate = baudrate;
    furi_hal_serial_set_br(ftdi_uart->serial_handle, baudrate);
}

void ftdi_uart_enable(FtdiUart* ftdi_uart, bool enable) {
    if(enable) {
        ftdi_uart->enable = true;
        furi_hal_serial_resume(ftdi_uart->serial_handle);
        furi_hal_serial_enable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionRx);
        furi_hal_serial_enable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionTx);
        ftdi_reset_purge_rx(ftdi_uart->ftdi);
        ftdi_reset_purge_tx(ftdi_uart->ftdi);
    } else {
        ftdi_uart->enable = false;
        furi_hal_serial_suspend(ftdi_uart->serial_handle);
        furi_hal_serial_disable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionRx);
        furi_hal_serial_disable_direction(ftdi_uart->serial_handle, FuriHalSerialDirectionTx);
    }
}

void ftdi_uart_set_data_config(FtdiUart* ftdi_uart, FtdiDataConfig* data_config) {
    furi_assert(data_config);
    UNUSED(ftdi_uart);

    bool is_uart_enabled = LL_LPUART_IsEnabled(LPUART1);

    if(is_uart_enabled) {
        LL_LPUART_Disable(LPUART1);
    }

    uint32_t data_width = LL_LPUART_DATAWIDTH_8B;
    uint32_t parity_mode = LL_LPUART_PARITY_NONE;
    uint32_t stop_bits_mode = LL_LPUART_STOPBITS_2;

    switch(data_config->BITS) {
    case FtdiBits7:
        data_width = LL_LPUART_DATAWIDTH_7B;
        break;
    case FtdiBits8:
        data_width = LL_LPUART_DATAWIDTH_8B;
        break;
    default:
        break;
    }

    switch(data_config->PARITY) {
    case FtdiParityNone:
        parity_mode = LL_LPUART_PARITY_NONE;
        break;
    case FtdiParityOdd:
        parity_mode = LL_LPUART_PARITY_ODD;
        break;
    case FtdiParityEven:
    case FtdiParityMark:
    case FtdiParitySpace:
        parity_mode = LL_LPUART_PARITY_EVEN;
        break;
    default:
        break;
    }

    switch(data_config->STOP_BITS) {
    case FtdiStopBits1:
        stop_bits_mode = LL_LPUART_STOPBITS_1;
        break;
    case FtdiStopBits15:
    case FtdiStopBits2:
        stop_bits_mode = LL_LPUART_STOPBITS_2;
        break;
    default:
        break;
    }

    LL_LPUART_ConfigCharacter(LPUART1, data_width, parity_mode, stop_bits_mode);
    if(is_uart_enabled) {
        LL_LPUART_Enable(LPUART1);
    }
}

static void ftdi_uart_dma_tx_isr(void* context) {
#if FTDI_UART_LPUART_DMA_CHANNEL == LL_DMA_CHANNEL_1
    FtdiUart* ftdi_uart = context;

    if(LL_DMA_IsActiveFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE)) {
        LL_DMA_ClearFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE);
        LL_DMA_DisableChannel(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL);
        //Todo It's a bad idea to wait for the end of the transfer in an interrupt...
        while(!LL_LPUART_IsActiveFlag_TC(LPUART1))
            ;
        furi_thread_flags_set(
            furi_thread_get_id(ftdi_uart->worker_thread), WorkerEventTXDataDmaEnd);
    }

#else
#error Update this code. Would you kindly?
#endif
}

static void ftdi_uart_tx_dma_init(FtdiUart* ftdi_uart) {
    ftdi_uart->buffer_tx_ptr = malloc(FTDI_UART_MAX_TXRX_SIZE);

    LL_DMA_SetPeriphAddress(
        FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL, (uint32_t) & (LPUART1->TDR));

    LL_DMA_ConfigTransfer(
        FTDI_UART_LPUART_DMA_INSTANCE,
        FTDI_UART_LPUART_DMA_CHANNEL,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_MODE_NORMAL | LL_DMA_PERIPH_NOINCREMENT |
            LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_BYTE | LL_DMA_MDATAALIGN_BYTE |
            LL_DMA_PRIORITY_HIGH);
    LL_DMA_SetPeriphRequest(
        FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL, LL_DMAMUX_REQ_LPUART1_TX);

    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma2Ch1, ftdi_uart_dma_tx_isr, ftdi_uart);

#if FTDI_UART_LPUART_DMA_CHANNEL == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_HT1(FTDI_UART_LPUART_DMA_INSTANCE))
        LL_DMA_ClearFlag_HT1(FTDI_UART_LPUART_DMA_INSTANCE);
    if(LL_DMA_IsActiveFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE))
        LL_DMA_ClearFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE);
    if(LL_DMA_IsActiveFlag_TE1(FTDI_UART_LPUART_DMA_INSTANCE))
        LL_DMA_ClearFlag_TE1(FTDI_UART_LPUART_DMA_INSTANCE);
#else
#error Update this code. Would you kindly?
#endif

    LL_DMA_EnableIT_TC(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL);
    LL_DMA_ClearFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE);
    LL_LPUART_EnableDMAReq_TX(LPUART1);
}

static void ftdi_uart_tx_dma_deinit(FtdiUart* ftdi_uart) {
    LL_DMA_DisableChannel(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL);
    LL_LPUART_DisableDMAReq_TX(LPUART1);
    LL_DMA_DisableIT_TC(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL);
    LL_DMA_ClearFlag_TC1(FTDI_UART_LPUART_DMA_INSTANCE);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma2Ch1, NULL, NULL);
    free(ftdi_uart->buffer_tx_ptr);
}

static void ftdi_uart_tx_dma(FtdiUart* ftdi_uart, uint8_t* data, size_t size) {
    UNUSED(ftdi_uart);
    LL_DMA_SetMemoryAddress(
        FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL, (uint32_t)data);
    LL_DMA_SetDataLength(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL, size);

    LL_DMA_EnableChannel(FTDI_UART_LPUART_DMA_INSTANCE, FTDI_UART_LPUART_DMA_CHANNEL);
}