#include "app.h"
#include "clock_model.h"

#include <gui/canvas.h>
#include <locale/locale.h>
#include <stdlib.h>
#include <string.h>

static void app_refresh_clock_strings(App* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    ClockModelInput input = {
        .hour = dt.hour,
        .minute = dt.minute,
        .second = dt.second,
        .utc_offset_minutes = app->settings.utc_offset_minutes,
        .hour_format_24 = (locale_get_time_format() == LocaleTimeFormat24h),
    };
    ClockModelSnapshot snap;
    clock_model_build_snapshot(&input, &snap);
    memcpy(app->beats_text, snap.beats_text, sizeof(app->beats_text));
    memcpy(app->local_time_text, snap.local_time_text, sizeof(app->local_time_text));
    settings_format_offset(
        app->settings.utc_offset_minutes, app->offset_text, sizeof(app->offset_text));
}

static void app_draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(app->screen == AppScreenClock) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, app->beats_text);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, app->local_time_text);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "UTC Offset");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, app->offset_text);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "< Left  Right >");
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "OK saves");
    }
}

static void app_input_callback(InputEvent* event, void* context) {
    App* app = context;
    AppEvent app_event = {.type = AppEventTypeInput, .input = *event};
    furi_message_queue_put(app->event_queue, &app_event, 0);
}

static void app_adjust_offset(App* app, int8_t step) {
    int16_t next =
        (int16_t)(app->settings.utc_offset_minutes + (step * INTERNET_TIME_OFFSET_STEP_MINUTES));
    if(next < INTERNET_TIME_OFFSET_MIN_MINUTES) {
        next = INTERNET_TIME_OFFSET_MIN_MINUTES;
    }
    if(next > INTERNET_TIME_OFFSET_MAX_MINUTES) {
        next = INTERNET_TIME_OFFSET_MAX_MINUTES;
    }
    app->settings.utc_offset_minutes = next;
    settings_format_offset(
        app->settings.utc_offset_minutes, app->offset_text, sizeof(app->offset_text));
}

static void app_open_settings(App* app) {
    app->screen = AppScreenSettings;
    settings_format_offset(
        app->settings.utc_offset_minutes, app->offset_text, sizeof(app->offset_text));
    view_port_update(app->view_port);
}

static void app_close_settings(App* app) {
    app->settings.loaded = true;
    if(!settings_save(app->storage, &app->settings)) {
        FURI_LOG_E("ITS", "settings_save failed");
    }
    app->screen = AppScreenClock;
    app_refresh_clock_strings(app);
    view_port_update(app->view_port);
}

static bool app_signal_callback(uint32_t signal, void* argument, void* context) {
    UNUSED(argument);
    App* app = context;
    if(signal == FuriSignalExit) {
        app->running = false;
        return true;
    }
    return false;
}

static bool app_handle_input(App* app, const InputEvent* event) {
    if(app->screen == AppScreenClock) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            app_open_settings(app);
        } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
            app->running = false;
        }
        return true;
    }

    if(event->key == InputKeyLeft &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app_adjust_offset(app, -1);
        view_port_update(app->view_port);
    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        app_adjust_offset(app, 1);
        view_port_update(app->view_port);
    } else if(
        (event->key == InputKeyOk || event->key == InputKeyBack) &&
        event->type == InputTypeShort) {
        app_close_settings(app);
    }
    return true;
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    furi_check(app);
    memset(app, 0, sizeof(App));

    app->storage = furi_record_open(RECORD_STORAGE);
    settings_init_defaults(&app->settings);
    (void)settings_load(app->storage, &app->settings);

    app->event_queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, app);
    view_port_enabled_set(app->view_port, true);

    app->running = true;
    if(!app->settings.loaded) {
        /* Persist UTC+0 default so relaunch opens the clock; user can change via OK. */
        app->settings.utc_offset_minutes = 0;
        app->settings.loaded = true;
        (void)settings_save(app->storage, &app->settings);
    }
    app->screen = AppScreenClock;
    app_refresh_clock_strings(app);
    return app;
}

static void app_free(App* app) {
    furi_check(app);

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t app_run(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    furi_thread_set_signal_callback(furi_thread_get_current(), app_signal_callback, app);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    AppEvent event;
    uint32_t last_second = 61;
    while(app->running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 200);
        if(status == FuriStatusOk && event.type == AppEventTypeInput) {
            app_handle_input(app, &event.input);
        }

        if(app->screen == AppScreenClock) {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            if(dt.second != last_second) {
                last_second = dt.second;
                app_refresh_clock_strings(app);
                view_port_update(app->view_port);
            }
        }
    }

    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);
    app_free(app);
    return 0;
}
