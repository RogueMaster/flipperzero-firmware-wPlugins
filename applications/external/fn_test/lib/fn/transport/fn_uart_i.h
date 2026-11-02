/**
 * @file fn_uart_i.h
 * @brief Приватное состояние транспорта ФН.
 */
#pragma once

#include "core/fn_frame.h"
#include "fn_uart.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <expansion/expansion.h>

#define FN_UART_BAUDRATE 115200u
#define FN_UART_BAUD_TIMEOUT_MS 300u
/** Длительность одного ожидания данных: чем меньше, тем быстрее отмена. */
#define FN_UART_SLICE_MS 20u

/**
 * Порт ФН.
 *
 * Синхронизация с DMA-RX выполнена через FuriStreamBuffer: его запись
 * (`furi_stream_buffer_send`) безопасна в прерывании и сама будит поток,
 * ожидающий в `furi_stream_buffer_receive`. Флаги потока для этого не
 * используются — `furi_thread_flags_set` из ISR недопустим.
 */
struct FnUart {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    Expansion* expansion;
    uint8_t dma_buf[FURI_HAL_SERIAL_DMA_BUFFER_SIZE];

    volatile bool busy;
    volatile bool rx_ready;
    volatile bool overflow;
    volatile bool cancel_requested;

#ifdef FN_TESTS
    /* Подмена ответа для on-device тестов (T070): позволяет проверить
     * обработку битого кадра без реального ФН и без порчи обмена. */
    uint8_t inject_buf[FN_FRAME_MAX_LEN];
    size_t inject_len;
#endif
};

/** Таймаут по умолчанию для одной команды. */
uint32_t fn_uart_default_timeout(void);

#ifdef FN_TESTS
/**
 * Подставить байты «ответа» на следующий обмен (только тестовая сборка).
 * Данные отдаются ровно один раз, затем транспорт снова работает с портом.
 */
void fn_uart_test_inject(FnUart* uart, const uint8_t* data, size_t len);
#endif
