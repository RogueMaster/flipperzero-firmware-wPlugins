#include "../period_tracker.h"
#include "../period_tracker_models.h"

typedef enum {
    DatePickerStepYear,
    DatePickerStepMonth,
    DatePickerStepDay,
} DatePickerStep;

static void date_picker_year_callback(void* context, int32_t number) {
    PeriodTrackerApp* app = context;
    app->temp_year = (uint16_t)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, DatePickerStepYear);
}

static void date_picker_month_callback(void* context, int32_t number) {
    PeriodTrackerApp* app = context;
    app->temp_month = (uint8_t)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, DatePickerStepMonth);
}

static void date_picker_day_callback(void* context, int32_t number) {
    PeriodTrackerApp* app = context;
    app->temp_day = (uint8_t)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, DatePickerStepDay);
}

void period_tracker_scene_date_picker_on_enter(void* context) {
    PeriodTrackerApp* app = context;
    furi_assert(app);

    // Initialize with today's date
    SimpleDate today;
    get_today(&today);
    app->temp_year = today.year;
    app->temp_month = today.month;
    app->temp_day = today.day;

    // Start with year selection
    number_input_set_header_text(app->number_input, "Select Year");
    number_input_set_result_callback(
        app->number_input, date_picker_year_callback, app, app->temp_year, 2000, 2099);

    view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
}

bool period_tracker_scene_date_picker_on_event(void* context, SceneManagerEvent event) {
    PeriodTrackerApp* app = context;
    furi_assert(app);
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DatePickerStepYear) {
            // Year selected, now ask for month
            number_input_set_header_text(app->number_input, "Select Month (1-12)");
            number_input_set_result_callback(
                app->number_input, date_picker_month_callback, app, app->temp_month, 1, 12);

            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
            consumed = true;
        } else if(event.event == DatePickerStepMonth) {
            // Month selected, now ask for day
            // Calculate max days in the selected month
            uint8_t max_days = datetime_get_days_per_month(
                datetime_is_leap_year(app->temp_year), app->temp_month);

            // Ensure current day is valid for the month
            if(app->temp_day > max_days) {
                app->temp_day = max_days;
            }

            char header[32];
            snprintf(header, sizeof(header), "Select Day (1-%d)", max_days);
            number_input_set_header_text(app->number_input, header);
            number_input_set_result_callback(
                app->number_input, date_picker_day_callback, app, app->temp_day, 1, max_days);

            view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewNumberInput);
            consumed = true;
        } else if(event.event == DatePickerStepDay) {
            // Day selected, validate the date and return to previous scene
            SimpleDate selected_date;
            selected_date.year = app->temp_year;
            selected_date.month = app->temp_month;
            selected_date.day = app->temp_day;

            if(is_date_valid(&selected_date)) {
                // Date is valid, return to calling scene with custom event
                scene_manager_previous_scene(app->scene_manager);
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, PERIOD_TRACKER_EVENT_DATE_SELECTED);
            } else {
                // Date invalid (shouldn't happen with our validation, but just in case)
                widget_reset(app->widget);
                widget_add_string_multiline_element(
                    app->widget,
                    64,
                    32,
                    AlignCenter,
                    AlignCenter,
                    FontPrimary,
                    "Error!\n\nInvalid date.");
                view_dispatcher_switch_to_view(app->view_dispatcher, PeriodTrackerViewWidget);
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
        }
    }

    return consumed;
}

void period_tracker_scene_date_picker_on_exit(void* context) {
    UNUSED(context);
}
