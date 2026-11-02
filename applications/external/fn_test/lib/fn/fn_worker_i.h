/**
 * @file fn_worker_i.h
 * @brief Приватное состояние воркера ФН.
 */
#pragma once

#include "core/fn_frame.h"
#include "fn_worker.h"
#include "transport/fn_uart.h"

#include <furi.h>

/** События потока воркера. */
typedef enum {
    FnWorkerEventStart = (1u << 0),
    FnWorkerEventStop = (1u << 1),
} FnWorkerEvent;

#define FN_WORKER_STACK_SIZE 10240u

struct FNWorker {
    FuriThread* thread;
    FnUart* uart;

    volatile bool busy;
    volatile bool cancel_requested;
    volatile uint32_t op_id;
    volatile uint32_t deadline_tick;

    FnOpKind kind;
    FnTask* task;
    FnWorkerCallback callback;
    void* callback_context;

    /* Буферы обмена: живут в куче, чтобы не раздувать стек воркера. */
    uint8_t tx_buf[FN_FRAME_MAX_LEN];
    uint8_t rx_buf[FN_FRAME_MAX_LEN];
};

/** Остаток дедлайна операции в миллисекундах (0, если истек). */
uint32_t fn_worker_deadline_left(const FNWorker* worker);

/** Проверка запроса отмены (используется fn_ops). */
bool fn_worker_cancelled(const FNWorker* worker);

/** Выполнить текущую операцию (реализация — fn_ops.c). */
void fn_ops_run(FNWorker* worker, FnOpResult* result);

#ifdef FN_TESTS
/**
 * Счётчик фактических отправок команды очистки МГМ (60h).
 * Используется on-device тестом «боевой ФН — 0 отправок» (T066, SC-009).
 */
extern volatile uint32_t fn_ops_flash_sends;
#endif
