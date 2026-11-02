#include "fn_worker_i.h"

#include "core/fn_frame.h"
#include "proto/fn_commands.h"
#include "proto/fn_ident.h"
#include "proto/fn_life.h"
#include "proto/fn_status.h"

#include <furi_hal_rtc.h>

#define TAG "FNOps"

#ifdef FN_TESTS
/* Определение счётчика отправок очистки МГМ (T066, SC-009). */
volatile uint32_t fn_ops_flash_sends = 0;
#endif

/** Отправить одну команду и разобрать ответ. */
static FnOpStatus fn_ops_command(
    FNWorker* worker,
    uint8_t command,
    const uint8_t* data,
    size_t data_len,
    uint32_t timeout_ms,
    FnFrame* frame,
    FNError* error) {
    *error = FNOk;

    if(fn_worker_cancelled(worker)) return FnOpStatusCancelled;
    if(fn_worker_deadline_left(worker) < FN_CMD_TIMEOUT_MS) {
        FURI_LOG_W(TAG, "дедлайн операции истек, команда %02X не отправлена", command);
        return FnOpStatusNoResponse;
    }

    FnFrameStatus frame_status = FnFrameOk;
    const size_t tx_len = fn_frame_build(
        command, data, data_len, worker->tx_buf, sizeof(worker->tx_buf), &frame_status);
    if(tx_len == 0) {
        FURI_LOG_E(TAG, "не удалось собрать кадр %02X (статус %d)", command, frame_status);
        return FnOpStatusInternalError;
    }

    size_t rx_len = 0;
    const FnUartTrxStatus trx = fn_uart_trx(
        worker->uart,
        worker->tx_buf,
        tx_len,
        worker->rx_buf,
        sizeof(worker->rx_buf),
        &rx_len,
        timeout_ms);

    if(trx == FnUartTrxCancelled) return FnOpStatusCancelled;
    if(trx != FnUartTrxOk) {
        FURI_LOG_W(TAG, "нет ответа на команду %02X", command);
        return FnOpStatusNoResponse;
    }

    frame_status = fn_frame_parse(worker->rx_buf, rx_len, frame);
    if(frame_status != FnFrameOk) {
        FURI_LOG_W(TAG, "битый кадр ответа на %02X (статус %d)", command, frame_status);
        return FnOpStatusBadFrame;
    }

    if(frame->code != FNOk) {
        *error = (FNError)frame->code;
        FURI_LOG_W(TAG, "ФН отказал на %02X: код %02X", command, frame->code);
        return FnOpStatusFnRefused;
    }
    return FnOpStatusOk;
}

/** Detect: 30h (обязательно) → 3Ah → 33h. */
static FnOpStatus fn_ops_detect(FNWorker* worker, FnOpResult* result) {
    FnInfo* info = worker->task->info;
    if(!info) return FnOpStatusInternalError;

    fn_info_reset(info);

    FnFrame frame;
    FNError error = FNOk;
    FnOpStatus status = FnOpStatusNoResponse;

    for(uint32_t attempt = 0; attempt < FN_DETECT_ATTEMPTS; ++attempt) {
        status = fn_ops_command(
            worker, FN_CMDGetFNStatus, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
        if(status != FnOpStatusNoResponse) break;
    }
    if(status != FnOpStatusOk) {
        result->error = error;
        return status;
    }
    if(!fn_parse_status_30h(frame.data, frame.data_len, &info->status)) {
        FURI_LOG_W(TAG, "короткий ответ 30h: %u байт", (unsigned)frame.data_len);
        return FnOpStatusBadFrame;
    }
    result->has_warning = frame.has_warning;
    info->response_has_warning = frame.has_warning;

    status = fn_ops_command(worker, FN_CMDGetFFD, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status == FnOpStatusCancelled) return status;
    if(status == FnOpStatusOk &&
       fn_parse_ffd_3ah(frame.data, frame.data_len, &info->ffd)) {
        info->has_ffd = true;
        if(frame.has_warning) info->response_has_warning = true;
    } else {
        FURI_LOG_W(TAG, "ФФД (3Ah) недоступен: статус %d", (int)status);
    }

    status = fn_ops_command(
        worker, FN_CMDGetFNFwVersion, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status == FnOpStatusCancelled) return status;
    if(status == FnOpStatusOk && fn_parse_fw_33h(frame.data, frame.data_len, &info->fw)) {
        info->has_fw = true;
        if(frame.has_warning) info->response_has_warning = true;
    } else {
        FURI_LOG_W(TAG, "версия ПО (33h) недоступна: статус %d", (int)status);
    }

    return FnOpStatusOk;
}

/** ReadInfo: Detect + 10h только для этапа 2. */
static FnOpStatus fn_ops_read_info(FNWorker* worker, FnOpResult* result) {
    FnInfo* info = worker->task->info;
    if(!info) return FnOpStatusInternalError;

    const FnOpStatus detect = fn_ops_detect(worker, result);
    if(detect != FnOpStatusOk) return detect;

    if(info->status.stage != FnStageRegistered) return FnOpStatusOk;

    FnFrame frame;
    FNError error = FNOk;
    const FnOpStatus status = fn_ops_command(
        worker, FN_CMDGetFNSessionInfo, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status == FnOpStatusCancelled) return status;
    if(status == FnOpStatusOk && fn_parse_session_10h(frame.data, frame.data_len, &info->session)) {
        info->has_session = true;
        if(frame.has_warning) info->response_has_warning = true;
    } else {
        FURI_LOG_W(TAG, "смена (10h) недоступна: статус %d", (int)status);
    }
    return FnOpStatusOk;
}

/** ReadLife: 32h (обязательно) → 3Dh → 3Bh. */
static FnOpStatus fn_ops_read_life(FNWorker* worker, FnOpResult* result) {
    FnLifeInfo* life = worker->task->life;
    if(!life) return FnOpStatusInternalError;

    fn_life_info_reset(life);

    FnFrame frame;
    FNError error = FNOk;

    FnOpStatus status = fn_ops_command(
        worker, FN_CMDGetFNEndDate, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status != FnOpStatusOk) {
        result->error = error;
        return status;
    }
    if(!fn_parse_end_date_32h(frame.data, frame.data_len, &life->date)) {
        return FnOpStatusBadFrame;
    }
    life->has_date = true;
    result->has_warning = frame.has_warning;

    status = fn_ops_command(
        worker, FN_CMDGetFNMemoryInfo, NULL, 0, FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status == FnOpStatusCancelled) return status;
    if(status == FnOpStatusOk && fn_parse_memory_3dh(frame.data, frame.data_len, &life->memory)) {
        life->has_memory = true;
        if(frame.has_warning) result->has_warning = true;
    } else {
        FURI_LOG_W(TAG, "ресурс памяти (3Dh) недоступен: статус %d", (int)status);
    }

    DateTime rtc;
    furi_hal_rtc_get_datetime(&rtc);
    const uint8_t dt_data[5] = {
        (uint8_t)(rtc.year % 100),
        rtc.month,
        rtc.day,
        rtc.hour,
        rtc.minute,
    };
    status = fn_ops_command(
        worker, FN_CMDGetFNEndDays, dt_data, sizeof(dt_data), FN_CMD_TIMEOUT_MS, &frame, &error);
    if(status == FnOpStatusCancelled) return status;
    if(status == FnOpStatusOk && fn_parse_end_days_3bh(frame.data, frame.data_len, &life->days)) {
        life->has_days = life->days.has_days;
        if(frame.has_warning) result->has_warning = true;
    } else {
        FURI_LOG_W(TAG, "остаток дней (3Bh) недоступен: статус %d", (int)status);
    }

    return FnOpStatusOk;
}

/** FlashMgm: ровно одна отправка 60h+16h без повторов. */
static FnOpStatus fn_ops_flash_mgm(FNWorker* worker, FnOpResult* result) {
    const FnInfo* info = worker->task->info;
    /* Защита в глубину: команда уходит только в ФН, подтверждённый как МГМ
     * данными 33h (FR-024, SC-009). Признак не подтверждён — не собираем кадр. */
    if(!info || !info->has_fw || !info->fw.is_mgm) {
        FURI_LOG_E(TAG, "очистка МГМ запрещена: ФН не определён как МГМ");
        return FnOpStatusInternalError;
    }

    const uint8_t param = FN_CMD_FLASH_MGM_PARAM;
    FnFrame frame;
    FNError error = FNOk;

#ifdef FN_TESTS
    /* Счётчик отправок для on-device теста (T066): проверка «0 отправок». */
    fn_ops_flash_sends++;
#endif

    const FnOpStatus status = fn_ops_command(
        worker, FN_CMDFlashMGM, &param, 1, FN_OP_DEADLINE_FLASH_MS, &frame, &error);
    if(status != FnOpStatusOk) {
        result->error = error;
        return status;
    }
    result->has_warning = frame.has_warning;
    return FnOpStatusOk;
}

void fn_ops_run(FNWorker* worker, FnOpResult* result) {
    if(!worker || !result) return;

    switch(worker->kind) {
    case FnOpDetect:
        result->status = fn_ops_detect(worker, result);
        break;
    case FnOpReadInfo:
        result->status = fn_ops_read_info(worker, result);
        break;
    case FnOpReadLife:
        result->status = fn_ops_read_life(worker, result);
        break;
    case FnOpFlashMgm:
        result->status = fn_ops_flash_mgm(worker, result);
        break;
    default:
        result->status = FnOpStatusInternalError;
        break;
    }

    if(worker->cancel_requested && fn_op_is_cancellable(worker->kind)) {
        result->status = FnOpStatusCancelled;
        result->error = FNOk;
    }
}
