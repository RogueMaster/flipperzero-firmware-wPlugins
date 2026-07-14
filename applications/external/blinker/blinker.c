#include "blinker.h"

static void timer_callback(void* context) {
    BlinkerApp* app = context;

    static bool led_state = false;

    led_state = !led_state;
    furi_hal_light_set(LightRed, led_state ? 255 : 0);
    furi_hal_light_set(LightGreen, led_state ? 255 : 0);
    furi_hal_light_set(LightBlue, led_state ? 255 : 0);

    if(!app->time_out) {
        uint32_t current_time = furi_get_tick() / 1000; // seconds
        uint32_t elapsed_time = (furi_get_tick() - app->start_time) / 1000; // seconds

        bool elapsed_time_passed = elapsed_time >= app->duration * 60;
        if(elapsed_time_passed) {
            app->time_out = true;
            elapsed_time = app->duration * 60;
        }

        // Gradually decrease the interval from max to min during the duration,
        // thanks to elapsed time.
        uint32_t interval =
            app->max_interval -
            (elapsed_time * (app->max_interval - app->min_interval) / (app->duration * 60));
        // Execute on first run and afterwards every 5 seconds.
        // Do not run after 'duration' has passed, which means after time_out.

        bool start = app->last_check == 0;
        bool time_for_new_check = app->last_check + 5 < current_time; // Run every 5 seconds

        if(start || time_for_new_check || app->time_out) {
            app->last_check = current_time;

            // Equation: 1 minute in miliseconds divided by number of cycles,
            // multiplied by 2 (on and off)
            uint32_t blink_interval = 60 * 1000 / (interval * 2);

            char text[32];
            snprintf(text, sizeof(text), "BPM: %lu", interval);
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Blinking");
            widget_add_string_element(
                app->widget, 64, 42, AlignCenter, AlignCenter, FontSecondary, text);

            furi_timer_restart(app->timer, blink_interval);
        }
    }
}

static bool back_button_callback(void* context) {
    BlinkerApp* app = context;

    if(app->current_view == Exec) {
        furi_timer_stop(app->timer);
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightBlue, 0);
    }

    bool back_to_main_menu = app->current_view != Main;
    if(back_to_main_menu) {
        app->current_view = Main;
        view_dispatcher_switch_to_view(app->view_dispatcher, Main);
        return true;
    }

    return false;
}

static void exec_view(BlinkerApp* app) {
    app->start_time = furi_get_tick();
    app->last_check = 0; // Initially set to constant 0.
    app->time_out = false;

    furi_timer_start(app->timer, 60 * 1000 / (app->max_interval * 2));
    timer_callback(app);

    app->current_view = Exec;
    view_dispatcher_switch_to_view(app->view_dispatcher, app->current_view);
}

static void number_picker_callback(void* context, int32_t value) {
    BlinkerApp* app = context;

    switch(app->mode) {
    case Max:
        app->max_interval = value;
        app->mode = Min;
        number_picker_view(app, "Min interval (bpm)", app->min_interval, 1, 200);
        break;
    case Min:
        app->min_interval = value;
        main_view(app);
        break;
    case Dur:
        app->duration = value;
        main_view(app);
        break;
    }
}

static void number_picker_view(
    BlinkerApp* app,
    const char* header,
    uint32_t current,
    uint32_t min,
    uint32_t max) {
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, number_picker_callback, app, current, min, max);

    app->current_view = NumberPicker;
    view_dispatcher_switch_to_view(app->view_dispatcher, app->current_view);
}

static void main_callback(DialogExResult result, void* context) {
    BlinkerApp* app = context;

    switch(result) {
    case DialogExResultLeft:
    case DialogExPressLeft:
        app->mode = Max;
        number_picker_view(app, "Max interval (bpm)", app->max_interval, 1, 200);
        break;

    case DialogExResultRight:
    case DialogExPressRight:
        app->mode = Dur;
        number_picker_view(app, "Duration (min)", app->duration, 1, 60);
        break;

    case DialogExResultCenter:
    case DialogExPressCenter:
        exec_view(app);
        break;

    case DialogExReleaseLeft:
    case DialogExReleaseRight:
    case DialogExReleaseCenter:
        // Handle button releases - no action needed
        break;
    }
}

static void main_view(BlinkerApp* app) {
    dialog_ex_set_header(app->dialog, "Blinker", 64, 14, AlignCenter, AlignCenter);

    char text[64];
    snprintf(
        text,
        sizeof(text),
        "Duration: %ld min\nInterval: %ld - %ld bpm",
        app->duration,
        app->max_interval,
        app->min_interval);
    dialog_ex_set_text(app->dialog, text, 64, 32, AlignCenter, AlignCenter);

    dialog_ex_set_left_button_text(app->dialog, "Int.");
    dialog_ex_set_center_button_text(app->dialog, "Flash");
    dialog_ex_set_right_button_text(app->dialog, "Dur.");

    app->current_view = Main;
    view_dispatcher_switch_to_view(app->view_dispatcher, app->current_view);
}

int32_t blinker_main(void* p) {
    UNUSED(p);
    BlinkerApp* app = malloc(sizeof(BlinkerApp));

    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    // Default values
    app->duration = 20;
    app->max_interval = 120;
    app->min_interval = 60;

    // Initialize GUI and dispatcher
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_button_callback);

    // Create and configure timer
    app->timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);

    // Initialize views
    app->dialog = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, main_callback);
    app->number_input = number_input_alloc();
    app->widget = widget_alloc();

    // Add views
    view_dispatcher_add_view(app->view_dispatcher, Main, dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(
        app->view_dispatcher, NumberPicker, number_input_get_view(app->number_input));
    view_dispatcher_add_view(app->view_dispatcher, Exec, widget_get_view(app->widget));

    // Run application
    main_view(app);
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    furi_timer_free(app->timer);
    view_dispatcher_remove_view(app->view_dispatcher, Main);
    view_dispatcher_remove_view(app->view_dispatcher, NumberPicker);
    view_dispatcher_remove_view(app->view_dispatcher, Exec);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    number_input_free(app->number_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
