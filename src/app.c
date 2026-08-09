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
        /* FontBigNumbers is digits-only; draw '@' with a text font. */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 34, 22, AlignCenter, AlignCenter, "@");
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 72, 22, AlignCenter, AlignCenter, app->beats_text + 1);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, app->local_time_text);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, "UTC Offset");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, app->offset_text);
        canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignCenter, "Left/Right  Back=OK");
    }
}

static void app_input_callback(InputEvent* event, void* context) {
    App* app = context;
    AppEvent app_event = {.type = AppEventTypeInput, .input = *event};
    furi_message_queue_put(app->event_queue, &app_event, 0);
}

static void app_tick_callback(void* context) {
    App* app = context;
    AppEvent app_event = {.type = AppEventTypeTick};
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
    (void)settings_save(app->storage, &app->settings);
    app->screen = AppScreenClock;
    app_refresh_clock_strings(app);
    view_port_update(app->view_port);
}

static bool app_handle_input(App* app, const InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return true;
    }

    if(app->screen == AppScreenClock) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            app_open_settings(app);
        } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
            app->running = false;
        }
        return true;
    }

    /* Settings screen */
    if(event->key == InputKeyLeft) {
        app_adjust_offset(app, -1);
        view_port_update(app->view_port);
    } else if(event->key == InputKeyRight) {
        app_adjust_offset(app, 1);
        view_port_update(app->view_port);
    } else if(event->key == InputKeyBack || event->key == InputKeyOk) {
        if(event->type == InputTypeShort) {
            app_close_settings(app);
        }
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

    app->event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, app);

    app->tick_timer = furi_timer_alloc(app_tick_callback, FuriTimerTypePeriodic, app);
    app->running = true;
    app->screen = app->settings.loaded ? AppScreenClock : AppScreenSettings;

    app_refresh_clock_strings(app);
    return app;
}

static void app_free(App* app) {
    furi_check(app);

    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);

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
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    furi_timer_start(app->tick_timer, 1000);
    view_port_update(app->view_port);

    AppEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == AppEventTypeTick) {
                if(app->screen == AppScreenClock) {
                    app_refresh_clock_strings(app);
                    view_port_update(app->view_port);
                }
            } else if(event.type == AppEventTypeInput) {
                app_handle_input(app, &event.input);
            }
        }
    }

    app_free(app);
    return 0;
}
