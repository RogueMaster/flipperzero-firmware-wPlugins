/**
 * @file fn_worker.h
 * @brief Воркер операций ФН: одна активная операция, дедлайны, отмена.
 *
 * Потребитель — только ui/ и fn_test_app.c. Результат доставляется
 * callback-ом из потока воркера ровно один раз (contracts/worker-events.md).
 */
#pragma once

#include "core/fn_error.h"
#include "proto/fn_info.h"
#include "proto/fn_life.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FN_CMD_TIMEOUT_MS 300u /**< ожидание ответа на одну команду */
#define FN_OP_DEADLINE_READ_MS 10000u /**< дедлайн операций чтения */
#define FN_OP_DEADLINE_FLASH_MS 30000u /**< дедлайн очистки МГМ */
#define FN_DETECT_ATTEMPTS 3u /**< попыток определения ФН */

/** Тип операции. */
typedef enum {
    FnOpDetect,
    FnOpReadInfo,
    FnOpReadLife,
    FnOpFlashMgm,
} FnOpKind;

/** Итог операции. */
typedef enum {
    FnOpStatusOk,
    FnOpStatusNoResponse,
    FnOpStatusFnRefused,
    FnOpStatusBadFrame,
    FnOpStatusCancelled,
    FnOpStatusBusy,
    FnOpStatusInternalError,
} FnOpStatus;

/**
 * Можно ли отменить операцию пользователем.
 *
 * Очистка МГМ необратима и не прерывается (FR-015, FR-023): Back во время
 * ожидания игнорируется, отмена не запрашивается ни сценой, ни воркером.
 */
static inline bool fn_op_is_cancellable(FnOpKind kind) {
    return kind != FnOpFlashMgm;
}

/** Результат завершенной операции. */
typedef struct {
    uint32_t id;
    FnOpKind kind;
    FnOpStatus status;
    FNError error;
    bool has_warning;
} FnOpResult;

/** Выходные данные операции; владелец — вызывающий. */
typedef struct {
    FnInfo* info; /**< обязателен для Detect/ReadInfo */
    FnLifeInfo* life; /**< обязателен для ReadLife */
} FnTask;

typedef struct FNWorker FNWorker;

/** Callback завершения операции (вызывается из потока воркера). */
typedef void (*FnWorkerCallback)(void* context, const FnOpResult* result);

FNWorker* fn_worker_alloc(void);

/** Освободить воркер; требует, чтобы операция не была активна. */
void fn_worker_free(FNWorker* worker);

/**
 * Запустить операцию, не блокируя вызывающий поток.
 * @return false, если воркер занят (callback не вызывается)
 */
bool fn_worker_start(
    FNWorker* worker,
    FnOpKind kind,
    FnTask* task,
    FnWorkerCallback callback,
    void* context);

/** Идентификатор запущенной операции (для сравнения с FnOpResult.id). */
uint32_t fn_worker_get_op_id(const FNWorker* worker);

/** Отменить операцию, не блокируя поток; для FlashMgm игнорируется. */
void fn_worker_cancel(FNWorker* worker);

bool fn_worker_is_busy(const FNWorker* worker);

#ifdef __cplusplus
}
#endif
