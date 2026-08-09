#include "app.h"
#include "settings_view.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>
#include <string.h>

static void app_tick_timer_callback(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppEventTick);
}

static bool app_custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if(event == AppEventTick) {
        clock_view_update(app->clock_view);
        return true;
    }
    return false;
}

static void app_open_settings(void* context) {
    App* app = context;
    settings_view_sync_from_app(app->settings_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSettings);
}

static uint32_t app_clock_navigation(void* context) {
    UNUSED(context);
    /* Back on clock exits the app. */
    return VIEW_NONE;
}

static uint32_t app_settings_navigation(void* context) {
    App* app = context;
    /* Ensure first-run selection is persisted when leaving settings. */
    app->settings.loaded = true;
    (void)settings_save(app->storage, &app->settings);
    clock_view_set_utc_offset(app->clock_view, app->settings.utc_offset_minutes);
    clock_view_update(app->clock_view);
    return AppViewClock;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    furi_check(app);
    memset(app, 0, sizeof(App));

    app->storage = furi_record_open(RECORD_STORAGE);
    settings_init_defaults(&app->settings);
    (void)settings_load(app->storage, &app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);

    app->clock_view = clock_view_alloc();
    clock_view_set_ok_callback(app->clock_view, app_open_settings, app);
    clock_view_set_utc_offset(app->clock_view, app->settings.utc_offset_minutes);
    view_set_previous_callback(clock_view_get_view(app->clock_view), app_clock_navigation);

    app->settings_view = settings_view_alloc(app);
    view_set_previous_callback(
        settings_view_get_view(app->settings_view), app_settings_navigation);
    view_set_context(settings_view_get_view(app->settings_view), app);

    view_dispatcher_add_view(
        app->view_dispatcher, AppViewClock, clock_view_get_view(app->clock_view));
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewSettings, settings_view_get_view(app->settings_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->tick_timer = furi_timer_alloc(app_tick_timer_callback, FuriTimerTypePeriodic, app);

    return app;
}

static void app_free(App* app) {
    furi_check(app);

    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewClock);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewSettings);

    clock_view_free(app->clock_view);
    settings_view_free(app->settings_view);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

int32_t app_run(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    if(app->settings.loaded) {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewClock);
        clock_view_update(app->clock_view);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSettings);
    }

    furi_timer_start(app->tick_timer, 1000);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
