/**
 * @file fn_uart.h
 * @brief Транспорт ФН: захват USART, DMA-прием, таймауты и отмена.
 *
 * Модуль используют только fn_worker/fn_ops. UI не обращается к транспорту.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Ожидание взвода RX DMA перед отправкой запроса. */
#define FN_RX_READY_WAIT_MS 100u

/** Итог обмена одним кадром. */
typedef enum {
    FnUartTrxOk = 0, /**< приняты байты ответа (не менее 6) */
    FnUartTrxTimeout, /**< ответа нет за отведенное время */
    FnUartTrxCancelled, /**< обмен отменен */
    FnUartTrxError, /**< порт не готов или занят */
} FnUartTrxStatus;

typedef struct FnUart FnUart;

/**
 * Захватить порт и запустить DMA-прием.
 * @return NULL, если порт занят (например, службой expansion) или память
 *         недоступна; приложение должно показать ошибку, а не падать.
 */
FnUart* fn_uart_alloc(void);

void fn_uart_free(FnUart* uart);

/**
 * Отправить кадр и дождаться ответа.
 * @param rx буфер вызывающего (не менее FN_FRAME_MAX_LEN)
 * @param rx_len фактическая длина ответа
 * @param timeout_ms общий таймаут обмена
 */
FnUartTrxStatus fn_uart_trx(
    FnUart* uart,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t timeout_ms);

/** Прервать текущий обмен (без блокировки); безопасно из другого потока. */
void fn_uart_cancel(FnUart* uart);

bool fn_uart_is_busy(const FnUart* uart);

#ifdef __cplusplus
}
#endif
