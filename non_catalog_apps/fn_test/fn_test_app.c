#include "fn_test_app_i.h"

#include "ui/scenes/fn_scene.h"

#ifdef FN_TESTS
#include "unit_tests/test_index.h"
#endif

#define WORKER_TAG TAG "App"

void fn_test_app_handle_worker_result(void* context, const FnOpResult* result) {
    furi_assert(context);
    furi_assert(result);
    FNApp* app = context;

    /* Результат записывается ДО события: очередь dispatcher-а обеспечивает
     * видимость данных в GUI-потоке (contracts/worker-events.md §1). */
    app->op_result = *result;
    view_dispatcher_send_custom_event(app->view_dispatcher, FNAppCustomEventWorkerResult);
}

bool fn_test_app_start_op(FNApp* app, FnOpKind kind) {
    furi_assert(app);

    app->pending_op = kind;
    memset(&app->op_result, 0, sizeof(app->op_result));

    /* Новый старт сразу делает недействительным id предыдущей операции:
     * если запуск не удался (воркер ещё завершает отменённую операцию),
     * её результат тоже не должен приниматься за текущий (T071, FR-030). */
    app->op_id++;
    app->op_result.id = app->op_id;
    app->op_result.kind = kind;

    if(!fn_worker_start(app->worker, kind, &app->task, fn_test_app_handle_worker_result, app)) {
        return false;
    }

    /* Авторитетный id — у воркера: по нему сцена отбрасывает «поздние»
     * результаты отменённых операций. */
    app->op_id = fn_worker_get_op_id(app->worker);
    app->op_result.id = app->op_id;
    return true;
}

static void fn_test_app_menu_callback(void* context, uint8_t index) {
    furi_assert(context);
    FNApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static bool fn_test_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    FNApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool fn_test_app_back_event_callback(void* context) {
    furi_assert(context);
    FNApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

FNApp* fn_test_app_alloc(void) {
    FURI_LOG_D(WORKER_TAG, "начало инициализации");

    FNApp* app = malloc(sizeof(FNApp));
    furi_check(app);
    memset(app, 0, sizeof(FNApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->fn_info = fn_info_alloc();
    app->fn_life_info = fn_life_info_alloc();
    if(!app->fn_info || !app->fn_life_info) {
        FURI_LOG_E(WORKER_TAG, "нет памяти под модели ФН");
        goto error;
    }
    app->task.info = app->fn_info;
    app->task.life = app->fn_life_info;

    app->view_menu = fn_view_menu_alloc();
    app->view_wait = fn_view_wait_alloc();
    app->view_text = fn_view_text_alloc();
    app->view_confirm = fn_view_confirm_alloc();
    if(!app->view_menu || !app->view_wait || !app->view_text || !app->view_confirm) {
        FURI_LOG_E(WORKER_TAG, "нет памяти под экраны");
        goto error;
    }
    fn_view_menu_set_callback(app->view_menu, fn_test_app_menu_callback, app);

    app->worker = fn_worker_alloc();
    if(!app->worker) {
        FURI_LOG_E(WORKER_TAG, "%s", FN_STR_PORT_BUSY);
        goto error;
    }

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&fn_scene_handlers, app);
    furi_check(app->view_dispatcher);
    furi_check(app->scene_manager);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fn_test_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fn_test_app_back_event_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, FNAppViewMenu, fn_view_menu_get_view(app->view_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, FNAppViewWait, fn_view_wait_get_view(app->view_wait));
    view_dispatcher_add_view(
        app->view_dispatcher, FNAppViewText, fn_view_text_get_view(app->view_text));
    view_dispatcher_add_view(
        app->view_dispatcher, FNAppViewConfirm, fn_view_confirm_get_view(app->view_confirm));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FNAppSceneStart);

#ifdef FN_TESTS
    fn_register_tests(app->worker);
    app->tests_registered = true;
#endif

    FURI_LOG_D(WORKER_TAG, "инициализация завершена");
    return app;

error:
    /* Освобождаем уже созданное и выходим без краша. */
    fn_test_app_free(app);
    return NULL;
}

void fn_test_app_free(FNApp* app) {
    if(!app) return;

#ifdef FN_TESTS
    if(app->tests_registered) {
        fn_unregister_tests();
        app->tests_registered = false;
    }
#endif

    if(app->view_dispatcher) {
        if(app->view_menu) view_dispatcher_remove_view(app->view_dispatcher, FNAppViewMenu);
        if(app->view_wait) view_dispatcher_remove_view(app->view_dispatcher, FNAppViewWait);
        if(app->view_text) view_dispatcher_remove_view(app->view_dispatcher, FNAppViewText);
        if(app->view_confirm) view_dispatcher_remove_view(app->view_dispatcher, FNAppViewConfirm);
    }

    fn_view_menu_free(app->view_menu);
    fn_view_wait_free(app->view_wait);
    fn_view_text_free(app->view_text);
    fn_view_confirm_free(app->view_confirm);

    if(app->worker) {
        if(!fn_worker_is_busy(app->worker)) {
            fn_worker_free(app->worker);
        } else {
            FURI_LOG_W(WORKER_TAG, "воркер занят, освобождение отложено");
        }
    }

    fn_info_free(app->fn_info);
    fn_life_info_free(app->fn_life_info);

    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    if(app->scene_manager) scene_manager_free(app->scene_manager);

    if(app->notifications) furi_record_close(RECORD_NOTIFICATION);
    if(app->gui) furi_record_close(RECORD_GUI);

    free(app);
    FURI_LOG_D(WORKER_TAG, "приложение освобождено");
}

int32_t fn_test_app(void* p) {
    UNUSED(p);

    FNApp* app = fn_test_app_alloc();
    if(!app) {
        FURI_LOG_E(WORKER_TAG, "%s", FN_STR_PORT_BUSY);
        return 1;
    }

    view_dispatcher_run(app->view_dispatcher);
    fn_test_app_free(app);
    return 0;
}
