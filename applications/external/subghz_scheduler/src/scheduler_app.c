#include "src/scheduler_app_i.h"

#include <applications/drivers/subghz/cc1101_ext/cc1101_ext_interconnect.h>
#include <lib/subghz/devices/devices.h>

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

static void scheduler_make_app_folder(SchedulerApp* app) {
    furi_assert(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_simply_mkdir(storage, SCHEDULER_APP_FOLDER)) {
        dialog_message_show_storage_error(app->dialogs, "Unable to create\napp folder");
    }
    furi_record_close(RECORD_STORAGE);
}

SchedulerApp* scheduler_app_alloc(void) {
    SchedulerApp* app = calloc(1, sizeof(SchedulerApp));

    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    app->tx_file_path = furi_string_alloc();

    app->save_dir = furi_string_alloc();

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

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SchedulerAppViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SchedulerAppViewPopup, popup_get_view(app->popup));

    app->menu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SchedulerAppViewMenu, submenu_get_view(app->menu));

    app->about_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SchedulerAppViewAbout, widget_get_view(app->about_widget));

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

    app->interval_view = scheduler_interval_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        SchedulerAppViewInterval,
        scheduler_interval_view_get_view(app->interval_view));
    scheduler_interval_view_set_app(app->interval_view, app);

    app->scheduler = scheduler_alloc();

    // Test for external device
    subghz_devices_init();
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_EXT_NAME);
    app->ext_radio_present = subghz_devices_is_connect(device);
    subghz_devices_deinit();
    if(app->ext_radio_present) {
        FURI_LOG_I(TAG, "External radio detected.");
        scheduler_set_radio(app->scheduler, 1);
    } else {
        FURI_LOG_I(TAG, "External radio not detected. Using internal only.");
        scheduler_set_radio(app->scheduler, 0);
    }

    app->should_reset = true;

    scene_manager_next_scene(app->scene_manager, SchedulerSceneMenu);

    return app;
}

void scheduler_app_free(SchedulerApp* app) {
    furi_assert(app);

    if(app->thread) {
        furi_thread_join(app->thread);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewInterval);
    scheduler_interval_view_free(app->interval_view);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewRunSchedule);
    scheduler_run_view_free(app->run_view);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewVarItemList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewAbout);
    widget_free(app->about_widget);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewMenu);
    submenu_free(app->menu);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, SchedulerAppViewPopup);
    popup_free(app->popup);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    scheduler_free(app->scheduler);

    furi_string_free(app->save_dir);
    furi_string_free(app->tx_file_path);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);

    expansion_enable(app->expansion);
    furi_record_close(RECORD_EXPANSION);

    free(app);
}

int32_t scheduler_app(void* p) {
    UNUSED(p);
    SchedulerApp* scheduler_app = scheduler_app_alloc();

    scheduler_make_app_folder(scheduler_app);

    view_dispatcher_run(scheduler_app->view_dispatcher);

    scheduler_app_free(scheduler_app);

    return 0;
}
