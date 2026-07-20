#include "../period_tracker.h"
#include "../period_tracker_models.h"
#include "../period_tracker_csv.h"

// After creating a new profile, optionally seed last period start (+ a couple earlier).

#define SEED_ITEM_ENTER_DATE 0
#define SEED_ITEM_SKIP_DONE  1

static void seed_history_submenu_callback(void* context, uint32_t index) {
    PeriodTrackerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void seed_history_go_main_menu(PeriodTrackerApp* app) {
    if(!scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, PeriodTrackerSceneMainMenu)) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, PeriodTrackerSceneMainMenu);
    }
}

static void seed_history_show_menu(PeriodTrackerApp* app) {
    submenu_reset(app->submenu);

    if(app->seed_period_count == 0) {
        submenu_set_header(app->submenu, "Period history");
        submenu_add_item(
            app->submenu,
            "Last period start",
            SEED_ITEM_ENTER_DATE,
            seed_history_submenu_callback,
            app);
        submenu_add_item(
            app->submenu, "Skip for now", SEED_ITEM_SKIP_DONE, seed_history_submenu_callback, app);
    } else if(app->seed_period_count < PERIOD_TRACKER_SEED_MAX_PERIODS) {
        char header[32];
        snprintf(header, sizeof(header), "Logged %u date%s", app->seed_period_count,
                 app->seed_period_count == 1 ? "" : "s");
        submenu_set_header(app->submenu, header);
        submenu_add_item(
            app->submenu,
            "Add earlier start",
            SEED_ITEM_ENTER_DATE,
            seed_history_submenu_callback,
            app);
        submenu_add_item(
            app->submenu, "Done", SEED_ITEM_SKIP_DONE, seed_history_submenu_callback, app);
    } else {
        // Should not stay on menu at max; Done path handles exit
        submenu_set_header(app->submenu, "History saved");
        submenu_add_item(
            app->submenu, "Done", SEED_ITEM_SKIP_DONE, seed_history_submenu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewSubmenu);
}

static bool seed_history_save_period(PeriodTrackerApp* app) {
    CycleEvent new_event;
    cycle_event_init(&new_event);
    new_event.date.year = app->temp_year;
    new_event.date.month = app->temp_month;
    new_event.date.day = app->temp_day;
    new_event.type = EventTypePeriodStart;
    new_event.value[0] = '\0';

    if(!is_date_valid(&new_event.date)) {
        period_tracker_widget_show_message(app, "Invalid date.\nTry again.", FontPrimary);
        return false;
    }

    if(!event_append(app->storage, app->current_girl_name, &new_event)) {
        period_tracker_widget_show_message(app, "Failed to save\nperiod date.", FontPrimary);
        return false;
    }

    app->seed_period_count++;

    char msg[96];
    char date_str[16];
    format_date_string(&new_event.date, date_str, sizeof(date_str));
    if(app->seed_period_count == 1) {
        snprintf(msg, sizeof(msg), "Last period saved\n\n%s", date_str);
    } else {
        snprintf(msg, sizeof(msg), "Period start saved\n\n%s", date_str);
    }
    period_tracker_widget_show_message(app, msg, FontPrimary);
    FURI_LOG_I(
        TAG,
        "Seed history: saved period start %s for %s (%u/%u)",
        date_str,
        app->current_girl_name,
        app->seed_period_count,
        PERIOD_TRACKER_SEED_MAX_PERIODS);
    return true;
}

void period_tracker_scene_seed_history_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    app->seed_period_count = 0;
    seed_history_show_menu(app);
}

bool period_tracker_scene_seed_history_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Back from menu = same as Skip/Done
        seed_history_go_main_menu(app);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PERIOD_TRACKER_EVENT_WIDGET_DISMISS) {
            if(app->seed_period_count >= PERIOD_TRACKER_SEED_MAX_PERIODS) {
                seed_history_go_main_menu(app);
            } else {
                seed_history_show_menu(app);
            }
            consumed = true;
        } else if(event.event == PERIOD_TRACKER_EVENT_DATE_SELECTED) {
            seed_history_save_period(app);
            consumed = true;
        } else if(event.event == SEED_ITEM_ENTER_DATE) {
            scene_manager_next_scene(app->scene_manager, PeriodTrackerSceneDatePicker);
            consumed = true;
        } else if(event.event == SEED_ITEM_SKIP_DONE) {
            seed_history_go_main_menu(app);
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_seed_history_on_exit(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
