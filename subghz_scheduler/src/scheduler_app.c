#include "src/scheduler_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <types.h>

#define TAG "Sub-GHzSchedulerApp"

static bool scheduler_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SchedulerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool scheduler_app_back_event_callback(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void scheduler_app_tick_event_callback(void* context) {
    furi_assert(context);
    SchedulerApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

SchedulerApp* scheduler_app_alloc(void) {
    SchedulerApp* app = malloc(sizeof(SchedulerApp));

    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->file_path = furi_string_alloc();

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&scheduler_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, scheduler_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, scheduler_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, scheduler_app_tick_event_callback, 500);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SchedulerAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->run_view = scheduler_run_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SchedulerAppViewRunSchedule,
        scheduler_run_view_get_view(app->run_view));

    app->scheduler = scheduler_alloc();
    //Scheduler* scheduler = app->scheduler;

    scene_manager_next_scene(app->scene_manager, SchedulerSceneStart);

    return app;
}

void scheduler_app_free(SchedulerApp* app) {
    furi_assert(app);
    scheduler_free(app->scheduler);

    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewRunSchedule);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    furi_string_free(app->file_path);
    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(app);
}

int32_t scheduler_app(void* p) {
    UNUSED(p);
    SchedulerApp* scheduler_app = scheduler_app_alloc();

    view_dispatcher_run(scheduler_app->view_dispatcher);

    scheduler_app_free(scheduler_app);

    return 0;
}
