#include "fn_worker_i.h"

#define TAG "FNWorker"

uint32_t fn_worker_deadline_left(const FNWorker* worker) {
    if(!worker) return 0;
    const uint32_t now = furi_get_tick();
    if(now >= worker->deadline_tick) return 0;
    const uint32_t ticks_left = worker->deadline_tick - now;
    // В SDK нет furi_ticks_to_ms(), переводим через частоту тика.
    return (uint32_t)(((uint64_t)ticks_left * 1000u) / furi_kernel_get_tick_frequency());
}

bool fn_worker_cancelled(const FNWorker* worker) {
    return worker && worker->cancel_requested;
}

static uint32_t fn_worker_deadline_ms(FnOpKind kind) {
    return (kind == FnOpFlashMgm) ? FN_OP_DEADLINE_FLASH_MS : FN_OP_DEADLINE_READ_MS;
}

static int32_t fn_worker_thread(void* context) {
    FNWorker* worker = context;

    for(;;) {
        const uint32_t flags = furi_thread_flags_wait(
            FnWorkerEventStart | FnWorkerEventStop, FuriFlagWaitAny, FuriWaitForever);

        if(flags & FnWorkerEventStop) break;
        if(!(flags & FnWorkerEventStart)) continue;

        FnOpResult result;
        result.id = worker->op_id;
        result.kind = worker->kind;
        result.status = FnOpStatusInternalError;
        result.error = FNOk;
        result.has_warning = false;

        FURI_LOG_I(TAG, "старт операции %u (тип %d)", (unsigned)result.id, (int)result.kind);
        worker->cancel_requested = false;
        worker->deadline_tick =
            furi_get_tick() + furi_ms_to_ticks(fn_worker_deadline_ms(worker->kind));

        fn_ops_run(worker, &result);

        FURI_LOG_I(
            TAG,
            "операция %u завершена: статус %d, код %02X",
            (unsigned)result.id,
            (int)result.status,
            (unsigned)result.error);

        if(worker->callback) {
            worker->callback(worker->callback_context, &result);
        }

        worker->busy = false;
        worker->task = NULL;
        worker->callback = NULL;
        worker->callback_context = NULL;
        worker->cancel_requested = false;
        furi_thread_flags_clear(FnWorkerEventStart);
    }

    return 0;
}

FNWorker* fn_worker_alloc(void) {
    FNWorker* worker = malloc(sizeof(FNWorker));
    if(!worker) return NULL;
    memset(worker, 0, sizeof(FNWorker));

    worker->uart = fn_uart_alloc();
    if(!worker->uart) {
        FURI_LOG_E(TAG, "не удалось занять порт ФН");
        free(worker);
        return NULL;
    }

    worker->thread = furi_thread_alloc_ex(
        "FNWorker", FN_WORKER_STACK_SIZE, fn_worker_thread, worker);
    furi_thread_set_priority(worker->thread, FuriThreadPriorityNormal);
    furi_thread_start(worker->thread);
    return worker;
}

void fn_worker_free(FNWorker* worker) {
    if(!worker) return;

    if(worker->busy) {
        FURI_LOG_W(TAG, "освобождение воркера при активной операции");
        worker->cancel_requested = true;
        fn_uart_cancel(worker->uart);
    }

    if(worker->thread) {
        furi_thread_flags_set(furi_thread_get_id(worker->thread), FnWorkerEventStop);
        furi_thread_join(worker->thread);
        furi_thread_free(worker->thread);
        worker->thread = NULL;
    }
    if(worker->uart) {
        fn_uart_free(worker->uart);
        worker->uart = NULL;
    }
    free(worker);
}

bool fn_worker_start(
    FNWorker* worker,
    FnOpKind kind,
    FnTask* task,
    FnWorkerCallback callback,
    void* context) {
    if(!worker || !task) return false;
    if(kind != FnOpFlashMgm && !task->info && !task->life) return false;
    if(worker->busy) return false;

    worker->busy = true;
    worker->cancel_requested = false;
    worker->kind = kind;
    worker->task = task;
    worker->callback = callback;
    worker->callback_context = context;
    worker->op_id++;

    furi_thread_flags_set(furi_thread_get_id(worker->thread), FnWorkerEventStart);
    return true;
}

uint32_t fn_worker_get_op_id(const FNWorker* worker) {
    return worker ? worker->op_id : 0;
}

void fn_worker_cancel(FNWorker* worker) {
    if(!worker || !worker->busy) return;
    if(!fn_op_is_cancellable(worker->kind)) {
        FURI_LOG_W(TAG, "отмена очистки МГМ запрещена");
        return;
    }
    worker->cancel_requested = true;
    fn_uart_cancel(worker->uart);
}

bool fn_worker_is_busy(const FNWorker* worker) {
    return worker && worker->busy;
}
