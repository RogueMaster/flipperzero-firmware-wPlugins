#include "../../fn_test_app_i.h"
#include "fn_scene.h"

/** Режим экрана ожидания по типу операции. */
static FnWaitMode fn_scene_wait_mode(FnOpKind kind) {
    switch(kind) {
    case FnOpFlashMgm:
        return FnWaitModeFlash;
    case FnOpReadInfo:
    case FnOpReadLife:
        return FnWaitModeRead;
    case FnOpDetect:
    default:
        return FnWaitModeDetect;
    }
}

/** Тексты ожидания: заголовок и подсказка подзаголовка. */
static void fn_scene_wait_texts(FnWaitMode mode, const char** title, const char** subtitle) {
    switch(mode) {
    case FnWaitModeFlash:
        *title = FN_STR_WAIT_FLASH;
        *subtitle = FN_STR_WAIT_FLASH_HINT;
        break;
    case FnWaitModeRead:
        *title = FN_STR_WAIT_READ;
        *subtitle = FN_STR_WAIT_READ_HINT;
        break;
    case FnWaitModeDetect:
    default:
        *title = FN_STR_WAIT_DETECT;
        *subtitle = FN_STR_WAIT_DETECT_HINT;
        break;
    }
}

void fn_scene_wait_on_enter(void* context) {
    furi_assert(context);
    FNApp* app = context;

    const FnOpKind kind = app->pending_op;
    const FnWaitMode mode = fn_scene_wait_mode(kind);
    /* Ожидание очистки МГМ не прерывается (FR-015, FR-023). */
    const bool cancelable = fn_op_is_cancellable(kind);

    /* Данные прошлой операции фазы жизни не должны попасть на новый экран. */
    if(kind == FnOpReadLife) fn_life_info_reset(app->fn_life_info);

    const char* title = NULL;
    const char* subtitle = NULL;
    fn_scene_wait_texts(mode, &title, &subtitle);
    fn_view_wait_set(app->view_wait, mode, title, subtitle, cancelable);

    view_dispatcher_switch_to_view(app->view_dispatcher, FNAppViewWait);

    app->result_reason = FnResultReasonOperation;

    if(!fn_test_app_start_op(app, kind)) {
        FURI_LOG_E(TAG, "не удалось запустить операцию %d: воркер занят", (int)kind);
        /* Результат «занято» доставляется тем же событием, что и обычный,
         * чтобы сцена показала понятный итог, а не зависла на ожидании. */
        app->op_result.id = app->op_id;
        app->op_result.kind = kind;
        app->op_result.status = FnOpStatusBusy;
        view_dispatcher_send_custom_event(app->view_dispatcher, FNAppCustomEventWorkerResult);
        return;
    }
}

/** Preflight «МГМ/боевой» после Detect: подтверждение только для МГМ (FR-024). */
static void fn_scene_wait_handle_flash_preflight(FNApp* app) {
    app->flash_preflight = false;

    const FnInfo* info = app->fn_info;
    const bool is_mgm = info->has_fw && info->fw.is_mgm;

    if(!is_mgm) {
        FURI_LOG_W(TAG, "очистка МГМ недоступна: ФН не определён как МГМ");
        app->result_reason = FnResultReasonNotMgm;
        scene_manager_next_scene(app->scene_manager, FNAppSceneResult);
        return;
    }

    scene_manager_next_scene(app->scene_manager, FNAppSceneConfirm);
}

/** Переход по результату операции: успех — данные (US1/US2), иначе — результат. */
static void fn_scene_wait_handle_result(FNApp* app) {
    if(app->op_result.status == FnOpStatusOk) {
        switch(app->op_result.kind) {
        case FnOpReadInfo:
            scene_manager_next_scene(app->scene_manager, FNAppSceneInfo);
            return;
        case FnOpReadLife:
            scene_manager_next_scene(app->scene_manager, FNAppSceneLife);
            return;
        case FnOpDetect:
            if(app->flash_preflight) {
                fn_scene_wait_handle_flash_preflight(app);
                return;
            }
            break;
        default:
            break;
        }
    }

    app->flash_preflight = false;
    app->result_reason = FnResultReasonOperation;
    scene_manager_next_scene(app->scene_manager, FNAppSceneResult);
}

bool fn_scene_wait_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FNApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == FNAppCustomEventWorkerResult) {
        if(app->op_result.id != app->op_id) {
            FURI_LOG_W(TAG, "поздний результат операции %u", (unsigned)app->op_result.id);
            return true;
        }
        fn_scene_wait_handle_result(app);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Back всегда потребляется сценой ожидания (FR-015, FR-018). */
        consumed = true;
        if(fn_op_is_cancellable(app->pending_op)) {
            /* Back во время чтения: отмена и немедленный возврат в меню,
             * не дожидаясь ответа ФН (SC-003, ≤ 1 с). */
            FURI_LOG_I(TAG, "ожидание отменено пользователем");
            fn_worker_cancel(app->worker);
            /* Отменённая операция может завершиться уже после ухода со сцены:
             * её id становится «чужим» и результат отбрасывается. */
            app->op_id++;
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, FNAppSceneStart);
        } else {
            /* Очистка МГМ не прерывается — ждём ответ или 30 с (FR-015, FR-023). */
            FURI_LOG_I(TAG, "Back проигнорирован: операция неотменяемая");
        }
    }

    return consumed;
}

void fn_scene_wait_on_exit(void* context) {
    furi_assert(context);
    FNApp* app = context;

    /* Анимация ожидания останавливается exit-callback-ом FnWaitView. */
    /* Очистку МГМ отменять запрещено (contracts/worker-events.md §3). */
    if(fn_op_is_cancellable(app->pending_op) && fn_worker_is_busy(app->worker)) {
        fn_worker_cancel(app->worker);
    }
}
