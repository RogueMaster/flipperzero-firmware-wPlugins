#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

typedef enum {
    LogEventMainMenu,
    LogEventSelectDate,
    LogEventSelectSymptom,
} LogEventStep;

// Menu item event ids
#define LOG_EVENT_ITEM_PERIOD      0
#define LOG_EVENT_ITEM_SYMPTOM     1
#define LOG_EVENT_ITEM_TODAY       10
#define LOG_EVENT_ITEM_YESTERDAY   11
#define LOG_EVENT_ITEM_CUSTOM_DATE 12
#define LOG_EVENT_SYMPTOM_BASE     100

static LogEventStep current_step = LogEventMainMenu;

static void period_tracker_scene_log_event_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Resolve the event date from the chosen date type (today/yesterday/custom)
static void log_event_resolve_date(PeriodTrackerApp* app, SimpleDate* date) {
    if(app->log_event_date_type == 2) {
        // Custom date from date picker
        date->year = app->temp_year;
        date->month = app->temp_month;
        date->day = app->temp_day;
    } else {
        get_today(date);
        if(app->log_event_date_type == 1) {
            add_days_to_date(date, -1); // Yesterday
        }
    }
}

static void log_event_show_date_menu(PeriodTrackerApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "When?");

    submenu_add_item(
        app->submenu,
        "Today",
        LOG_EVENT_ITEM_TODAY,
        period_tracker_scene_log_event_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Yesterday",
        LOG_EVENT_ITEM_YESTERDAY,
        period_tracker_scene_log_event_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Custom Date",
        LOG_EVENT_ITEM_CUSTOM_DATE,
        period_tracker_scene_log_event_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

static void log_event_show_symptom_menu(PeriodTrackerApp* app) {
    current_step = LogEventSelectSymptom;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Which symptom?");

    AppSettings settings;
    if(settings_load(app->storage, &settings)) {
        for(uint8_t i = 0; i < settings.symptom_count && i < MAX_SYMPTOMS; i++) {
            submenu_add_item(
                app->submenu,
                settings.global_symptoms[i],
                LOG_EVENT_SYMPTOM_BASE + i,
                period_tracker_scene_log_event_submenu_callback,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

static void log_event_log_period(PeriodTrackerApp* app) {
    CycleEvent new_event;
    cycle_event_init(&new_event);
    log_event_resolve_date(app, &new_event.date);
    new_event.type = EventTypePeriodStart;
    strcpy(new_event.value, "1");

    if(event_append(app->storage, app->current_girl_name, &new_event)) {
        widget_reset(app->widget);
        char msg[128];
        char date_str[16];
        format_date_string(&new_event.date, date_str, sizeof(date_str));
        snprintf(
            msg, sizeof(msg), "Period start logged!\n\n%s\n%s", app->current_girl_name, date_str);
        widget_add_string_multiline_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, msg);
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
        FURI_LOG_I(TAG, "Period start logged for %s (%s)", app->current_girl_name, date_str);
    } else {
        widget_reset(app->widget);
        widget_add_string_multiline_element(
            app->widget,
            64,
            32,
            AlignCenter,
            AlignCenter,
            FontPrimary,
            "Error!\n\nFailed to save event.");
        view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
    }
}

void period_tracker_scene_log_event_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    // Reset state
    current_step = LogEventMainMenu;

    // Show main menu: Period Start vs Log Symptom
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "What to log?");

    submenu_add_item(
        app->submenu,
        "Period Start",
        LOG_EVENT_ITEM_PERIOD,
        period_tracker_scene_log_event_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Log Symptom",
        LOG_EVENT_ITEM_SYMPTOM,
        period_tracker_scene_log_event_submenu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

bool period_tracker_scene_log_event_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type != SceneManagerEventTypeCustom) {
        return false;
    }

    if(event.event == PERIOD_TRACKER_EVENT_DATE_SELECTED) {
        // Returned from date picker with a custom date in temp_year/month/day
        app->log_event_date_type = 2;
        if(app->log_event_is_period) {
            log_event_log_period(app);
        } else {
            log_event_show_symptom_menu(app);
        }
        consumed = true;
    } else if(current_step == LogEventMainMenu) {
        if(event.event == LOG_EVENT_ITEM_PERIOD || event.event == LOG_EVENT_ITEM_SYMPTOM) {
            app->log_event_is_period = (event.event == LOG_EVENT_ITEM_PERIOD);
            current_step = LogEventSelectDate;
            log_event_show_date_menu(app);
            consumed = true;
        }
    } else if(current_step == LogEventSelectDate) {
        if(event.event == LOG_EVENT_ITEM_TODAY || event.event == LOG_EVENT_ITEM_YESTERDAY) {
            app->log_event_date_type = (event.event == LOG_EVENT_ITEM_TODAY) ? 0 : 1;
            if(app->log_event_is_period) {
                log_event_log_period(app);
            } else {
                log_event_show_symptom_menu(app);
            }
            consumed = true;
        } else if(event.event == LOG_EVENT_ITEM_CUSTOM_DATE) {
            app->log_event_date_type = 2;
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneDatePicker);
            consumed = true;
        }
    } else if(current_step == LogEventSelectSymptom) {
        if(event.event >= LOG_EVENT_SYMPTOM_BASE &&
           event.event < LOG_EVENT_SYMPTOM_BASE + MAX_SYMPTOMS) {
            CycleEvent new_event;
            cycle_event_init(&new_event);
            log_event_resolve_date(app, &new_event.date);
            new_event.type = EventTypeSymptom;

            AppSettings settings;
            if(settings_load(app->storage, &settings)) {
                uint8_t symptom_idx = event.event - LOG_EVENT_SYMPTOM_BASE;
                if(symptom_idx < settings.symptom_count) {
                    strncpy(
                        new_event.value,
                        settings.global_symptoms[symptom_idx],
                        MAX_SYMPTOM_NAME_LENGTH - 1);

                    if(event_append(app->storage, app->current_girl_name, &new_event)) {
                        widget_reset(app->widget);
                        char msg[256];
                        char date_str[16];
                        format_date_string(&new_event.date, date_str, sizeof(date_str));
                        snprintf(
                            msg,
                            sizeof(msg),
                            "Symptom logged!\n\n%s\n%s\n%s",
                            app->current_girl_name,
                            date_str,
                            new_event.value);
                        widget_add_string_multiline_element(
                            app->widget, 64, 32, AlignCenter, AlignCenter, FontSecondary, msg);
                        view_dispatcher_switch_to_view(
                            app->view_dispatcher, PeriodTrackerViewWidget);
                        FURI_LOG_I(
                            TAG,
                            "Symptom logged: %s for %s",
                            new_event.value,
                            app->current_girl_name);
                    }
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_log_event_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    widget_reset(app->widget);
    // Reset step for next time
    current_step = LogEventMainMenu;
}
