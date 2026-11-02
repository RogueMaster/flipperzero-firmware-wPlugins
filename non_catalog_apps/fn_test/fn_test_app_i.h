/**
 * @file fn_test_app_i.h
 * @brief Приватное состояние приложения FN Tester.
 */
#pragma once

#include "fn_test_app.h"

#include <fn_test_icons.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification_messages.h>

#include "lib/fn/fn.h"
#include "ui/fn_format.h"
#include "ui/fn_strings.h"
#include "ui/fn_text_model.h"
#include "ui/fn_ui.h"
#include "ui/views/fn_view_confirm.h"
#include "ui/views/fn_view_menu.h"
#include "ui/views/fn_view_text.h"
#include "ui/views/fn_view_wait.h"

#define TAG "FNTest"

/** События приложения. */
typedef enum {
    FNAppCustomEventWorkerResult = 0x100,
    FNAppCustomEventConfirmAccepted = 0x101,
    FNAppCustomEventConfirmDeclined = 0x102,
} FNAppCustomEvent;

/** Экраны приложения. */
typedef enum {
    FNAppViewMenu,
    FNAppViewWait,
    FNAppViewText,
    FNAppViewConfirm,
} FNAppView;

struct FNApp {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    FNWorker* worker;

    FnMenuView* view_menu;
    FnWaitView* view_wait;
    FnTextView* view_text;
    FnConfirmView* view_confirm;

    FnInfo* fn_info;
    FnLifeInfo* fn_life_info;
    FnTask task;
    FnOpResult op_result;
    FnOpKind pending_op; /**< операция, которую запускает сцена ожидания */
    bool flash_preflight; /**< Detect выполняется как проверка «МГМ/боевой» */
    FnResultReason result_reason; /**< причина показа экрана результата */
    volatile uint32_t op_id;

    FnTextModel text_model;

    bool tests_registered;
};

/**
 * Callback воркера: сохраняет результат и шлёт FNAppCustomEventWorkerResult.
 * Сцены передают его в fn_worker_start() (contracts/worker-events.md §1).
 */
void fn_test_app_handle_worker_result(void* context, const FnOpResult* result);

/**
 * Запустить операцию и запомнить её id (T071).
 *
 * Инкремент `op_id` выполняется при каждом старте: результат отменённой
 * операции приходит с прежним id и отбрасывается сценой ожидания.
 *
 * @return false, если воркер занят (результат «занято» доставляет сцена)
 */
bool fn_test_app_start_op(FNApp* app, FnOpKind kind);
