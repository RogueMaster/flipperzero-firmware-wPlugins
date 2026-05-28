/*
 * Flo Period Tracker for Flipper Zero
 * 
 * A menstrual cycle tracking app that helps track periods,
 * predict upcoming cycles, and show fertile windows.
 */

#include "flo_app.h"
#include "flo_views.h"

/* ── Menu callbacks ─────────────────────────────────────── */

static void flo_menu_callback(void* context, uint32_t index) {
    FloApp* app = context;
    switch(index) {
    case FloEventMenuStatus:
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewStatus);
        break;
    case FloEventMenuCalendar:
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewCalendar);
        break;
    case FloEventMenuLogPeriod:
        /* Reset the log view to today's date each time we enter */
        {
            FloDate today = flo_date_today();
            with_view_model(
                app->log_view,
                FloLogModel * model,
                {
                    model->date = today;
                    model->duration = app->data.default_period_duration;
                    model->field = 0;
                },
                true);
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewLogPeriod);
        break;
    case FloEventMenuSettings:
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewSettings);
        break;
    case FloEventMenuDeleteLast:
        flo_data_remove_last_period(&app->data);
        flo_data_save(&app->data);
        break;
    }
}

static uint32_t flo_navigation_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t flo_navigation_menu(void* context) {
    UNUSED(context);
    return FloViewMenu;
}

/* ── Settings callbacks ─────────────────────────────────── */

static void flo_settings_cycle_changed(VariableItem* item) {
    FloApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t cycle = 20 + index; /* range: 20-45 days */
    app->data.cycle_length = cycle;
    app->data.auto_cycle_length = false; /* manual override */

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", cycle);
    variable_item_set_current_value_text(item, buf);
}

static void flo_settings_duration_changed(VariableItem* item) {
    FloApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    uint8_t duration = 1 + index; /* range: 1-14 days */
    app->data.default_period_duration = duration;

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", duration);
    variable_item_set_current_value_text(item, buf);
}

/* ── Custom event handler (for OK button on Log view) ──── */

static bool flo_custom_event_callback(void* context, uint32_t event) {
    FloApp* app = context;

    if(event == FloEventLogSave) {
        /* Read the log model and save */
        FloDate date;
        uint8_t duration;
        with_view_model(
            app->log_view,
            FloLogModel * model,
            {
                date = model->date;
                duration = model->duration;
            },
            false);
        flo_data_add_period(&app->data, date, duration);
        flo_data_save(&app->data);
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewMenu);
        return true;
    } else if(event == FloEventLogCancel) {
        view_dispatcher_switch_to_view(app->view_dispatcher, FloViewMenu);
        return true;
    }
    return false;
}

/* ── App alloc/free ─────────────────────────────────────── */

static FloApp* flo_app_alloc(void) {
    FloApp* app = malloc(sizeof(FloApp));

    /* Load or initialize data */
    flo_data_load(&app->data);

    /* GUI setup */
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, flo_custom_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Main menu */
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Status", FloEventMenuStatus, flo_menu_callback, app);
    submenu_add_item(app->submenu, "Calendar", FloEventMenuCalendar, flo_menu_callback, app);
    submenu_add_item(app->submenu, "Log Period", FloEventMenuLogPeriod, flo_menu_callback, app);
    submenu_add_item(
        app->submenu, "Delete Last Period", FloEventMenuDeleteLast, flo_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", FloEventMenuSettings, flo_menu_callback, app);

    View* menu_view = submenu_get_view(app->submenu);
    view_set_previous_callback(menu_view, flo_navigation_exit);
    view_dispatcher_add_view(app->view_dispatcher, FloViewMenu, menu_view);

    /* Status view */
    app->status_view = flo_status_view_alloc(&app->data);
    view_set_previous_callback(app->status_view, flo_navigation_menu);
    view_dispatcher_add_view(app->view_dispatcher, FloViewStatus, app->status_view);

    /* Calendar view */
    app->calendar_view = flo_calendar_view_alloc(&app->data);
    view_set_previous_callback(app->calendar_view, flo_navigation_menu);
    view_dispatcher_add_view(app->view_dispatcher, FloViewCalendar, app->calendar_view);

    /* Log period view */
    app->log_view = flo_log_view_alloc(&app->data, app->view_dispatcher);
    view_set_previous_callback(app->log_view, flo_navigation_menu);
    view_dispatcher_add_view(app->view_dispatcher, FloViewLogPeriod, app->log_view);

    /* Settings */
    app->settings_list = variable_item_list_alloc();
    VariableItem* item_cycle = variable_item_list_add(
        app->settings_list, "Cycle Length", 26, flo_settings_cycle_changed, app);
    variable_item_set_current_value_index(item_cycle, app->data.cycle_length - 20);
    char cycle_buf[8];
    snprintf(cycle_buf, sizeof(cycle_buf), "%u", app->data.cycle_length);
    variable_item_set_current_value_text(item_cycle, cycle_buf);

    VariableItem* item_dur = variable_item_list_add(
        app->settings_list, "Period Duration", 14, flo_settings_duration_changed, app);
    variable_item_set_current_value_index(item_dur, app->data.default_period_duration - 1);
    char dur_buf[8];
    snprintf(dur_buf, sizeof(dur_buf), "%u", app->data.default_period_duration);
    variable_item_set_current_value_text(item_dur, dur_buf);

    View* settings_view = variable_item_list_get_view(app->settings_list);
    view_set_previous_callback(settings_view, flo_navigation_menu);
    view_dispatcher_add_view(app->view_dispatcher, FloViewSettings, settings_view);

    return app;
}

static void flo_app_free(FloApp* app) {
    /* Save data on exit */
    flo_data_save(&app->data);

    /* Remove views */
    view_dispatcher_remove_view(app->view_dispatcher, FloViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FloViewStatus);
    view_dispatcher_remove_view(app->view_dispatcher, FloViewCalendar);
    view_dispatcher_remove_view(app->view_dispatcher, FloViewLogPeriod);
    view_dispatcher_remove_view(app->view_dispatcher, FloViewSettings);

    /* Free views */
    submenu_free(app->submenu);
    view_free(app->status_view);
    view_free(app->calendar_view);
    view_free(app->log_view);
    variable_item_list_free(app->settings_list);

    /* Free dispatcher */
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ── Entry point ────────────────────────────────────────── */

int32_t flo_tracker_app(void* p) {
    UNUSED(p);

    FloApp* app = flo_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, FloViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    flo_app_free(app);
    return 0;
}
