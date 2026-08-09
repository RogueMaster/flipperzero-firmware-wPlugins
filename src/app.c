#include "app.h"
#include "clock_model.h"
#include "dcf77_decode.h"
#include "dcf77_time.h"
#include "u8g2_fonts.h"

#include <datetime/datetime.h>
#include <furi_hal_resources.h>
#include <gui/canvas.h>
#include <locale/locale.h>
#include <stdlib.h>
#include <string.h>

static const GpioPin* const k_dcf77_pin = &gpio_ext_pc0;

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

static void app_commit_model(App* app, bool update) {
    with_view_model(
        app->view,
        AppViewModel * model,
        {
            model->screen = app->screen;
            model->settings_row = app->settings_row;
            memcpy(model->beats_text, app->beats_text, sizeof(model->beats_text));
            memcpy(model->local_time_text, app->local_time_text, sizeof(model->local_time_text));
            memcpy(model->offset_text, app->offset_text, sizeof(model->offset_text));
            memcpy(model->sync_status, app->sync_status, sizeof(model->sync_status));
            memcpy(model->sync_detail, app->sync_detail, sizeof(model->sync_detail));
        },
        update);
}

static void
    app_draw_str_centered_x(Canvas* canvas, int32_t screen_w, int32_t cy, const char* text) {
    const uint16_t text_w = canvas_string_width(canvas, text);
    int32_t x = (screen_w - (int32_t)text_w) / 2;
    if(x < 0) {
        x = 0;
    }
    if(x + (int32_t)text_w > screen_w) {
        x = screen_w - (int32_t)text_w;
        if(x < 0) {
            x = 0;
        }
    }
    canvas_draw_str_aligned(canvas, x, cy, AlignLeft, AlignCenter, text);
}

static void app_draw_callback(Canvas* canvas, void* model_ptr) {
    AppViewModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    const int32_t screen_w = (int32_t)canvas_width(canvas);

    if(model->screen == AppScreenClock) {
        const int32_t beats_cy = 24;
        const int32_t time_cy = 54;

        canvas_set_custom_u8g2_font(canvas, u8g2_font_logisoso28_tr);
        const uint16_t beats_w = canvas_string_width(canvas, model->beats_text);
        const int32_t min_left = 6;
        int32_t beats_x = (screen_w - (int32_t)beats_w) / 2;
        if(beats_x < min_left) {
            beats_x = min_left;
        }
        const int32_t axis_x = beats_x + (int32_t)(beats_w / 2);
        canvas_draw_str_aligned(
            canvas, beats_x, beats_cy, AlignLeft, AlignCenter, model->beats_text);

        canvas_set_custom_u8g2_font(canvas, u8g2_font_logisoso16_tr);
        const uint16_t time_w = canvas_string_width(canvas, model->local_time_text);
        int32_t time_x = axis_x - (int32_t)(time_w / 2);
        if(time_x < 0) {
            time_x = 0;
        }
        canvas_draw_str_aligned(
            canvas, time_x, time_cy, AlignLeft, AlignCenter, model->local_time_text);
    } else if(model->screen == AppScreenSync) {
        canvas_set_font(canvas, FontPrimary);
        app_draw_str_centered_x(canvas, screen_w, 12, "DCF77 Sync");
        canvas_set_font(canvas, FontSecondary);
        app_draw_str_centered_x(canvas, screen_w, 28, model->sync_status);
        app_draw_str_centered_x(canvas, screen_w, 42, model->sync_detail);
        app_draw_str_centered_x(canvas, screen_w, 58, "Back cancel  LongOK demo");
    } else {
        canvas_set_font(canvas, FontSecondary);
        char line[28];
        const char* rows[AppSettingsRowCount];
        rows[AppSettingsRowOffset] = model->offset_text;
        rows[AppSettingsRowAuto] = model->sync_status; /* reused: "Auto: On/Off" */
        rows[AppSettingsRowInvert] = model->sync_detail; /* "Inv: On/Off" */
        rows[AppSettingsRowSync] = "OK: Start sync";

        for(uint8_t i = 0; i < AppSettingsRowCount; i++) {
            snprintf(
                line,
                sizeof(line),
                "%c %s",
                (i == (uint8_t)model->settings_row) ? '>' : ' ',
                rows[i]);
            canvas_draw_str(canvas, 2, (int32_t)(12 + i * 12), line);
        }
        canvas_draw_str_aligned(
            canvas, screen_w / 2, 62, AlignCenter, AlignCenter, "Up/Dn  L/R  Back save");
    }
}

static bool app_view_input_callback(InputEvent* event, void* context) {
    App* app = context;
    if(!app->running) {
        return false;
    }
    AppEvent app_event = {.type = AppEventTypeInput, .input = *event};
    (void)furi_message_queue_put(app->event_queue, &app_event, 0);
    return true;
}

static void app_request_exit(App* app) {
    if(!app->running) {
        return;
    }
    app->running = false;
    AppEvent exit_event = {.type = AppEventTypeExit};
    (void)furi_message_queue_put(app->event_queue, &exit_event, 0);
}

static void app_back_callback(void* context) {
    App* app = context;
    if(!app->running) {
        return;
    }
    if(app->screen == AppScreenSettings || app->screen == AppScreenSync) {
        AppEvent app_event = {
            .type = AppEventTypeInput,
            .input = {.key = InputKeyBack, .type = InputTypeShort},
        };
        (void)furi_message_queue_put(app->event_queue, &app_event, 0);
    } else {
        app_request_exit(app);
    }
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

static void app_refresh_settings_labels(App* app) {
    settings_format_offset(
        app->settings.utc_offset_minutes, app->offset_text, sizeof(app->offset_text));
    snprintf(
        app->sync_status,
        sizeof(app->sync_status),
        "Auto: %s",
        app->settings.auto_dcf77_sync ? "On" : "Off");
    snprintf(
        app->sync_detail,
        sizeof(app->sync_detail),
        "Inv: %s",
        app->settings.dcf77_invert ? "On" : "Off");
}

static void app_gpio_init(App* app) {
    furi_hal_gpio_init(k_dcf77_pin, GpioModeInput, GpioPullUp, GpioSpeedLow);
    app->gpio_have_level = false;
    app->gpio_edge_tick = 0;
    app->gpio_last_tick = furi_get_tick();
    dcf77_bit_buffer_reset(&app->dcf77_buf);
}

static void app_gpio_deinit(void) {
    furi_hal_gpio_init(k_dcf77_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static bool app_apply_civil_time(App* app, const Dcf77CivilTime* civil) {
    Dcf77DateTime local;
    if(!dcf77_civil_to_local(civil, app->settings.utc_offset_minutes, &local)) {
        return false;
    }
    DateTime dt = {
        .second = local.second,
        .minute = local.minute,
        .hour = local.hour,
        .day = local.day,
        .month = local.month,
        .year = local.year,
        .weekday = local.weekday,
    };
    if(!datetime_validate_datetime(&dt)) {
        return false;
    }
    furi_hal_rtc_set_datetime(&dt);
    app->settings.last_dcf77_sync_epoch = datetime_datetime_to_timestamp(&dt);
    (void)settings_save(app->storage, &app->settings);
    return true;
}

static void app_sync_demo_inject(App* app) {
    /* Fixture: CEST 2026-08-10 12:00 — proves RTC write path without RF. */
    Dcf77CivilTime civil = {
        .minute = 0,
        .hour = 12,
        .day = 10,
        .month = 8,
        .year = 2026,
        .weekday = 1,
        .cest = true,
    };
    if(app_apply_civil_time(app, &civil)) {
        snprintf(app->sync_status, sizeof(app->sync_status), "Demo OK");
        snprintf(app->sync_detail, sizeof(app->sync_detail), "RTC set (fixture)");
    } else {
        snprintf(app->sync_status, sizeof(app->sync_status), "Demo fail");
        snprintf(app->sync_detail, sizeof(app->sync_detail), "bad convert");
    }
    app_commit_model(app, true);
}

static void app_open_sync(App* app) {
    app->screen = AppScreenSync;
    app_gpio_init(app);
    snprintf(app->sync_status, sizeof(app->sync_status), "Listening C0");
    snprintf(app->sync_detail, sizeof(app->sync_detail), "bits 0/59");
    app_commit_model(app, true);
}

static void app_close_sync(App* app, bool to_settings) {
    app_gpio_deinit();
    app->screen = to_settings ? AppScreenSettings : AppScreenClock;
    if(app->screen == AppScreenSettings) {
        app_refresh_settings_labels(app);
    } else {
        app_refresh_clock_strings(app);
    }
    app_commit_model(app, true);
}

static void app_open_settings(App* app) {
    app->screen = AppScreenSettings;
    app->settings_row = AppSettingsRowOffset;
    app_refresh_settings_labels(app);
    app_commit_model(app, true);
}

static void app_close_settings(App* app) {
    app->settings.loaded = true;
    if(!settings_save(app->storage, &app->settings)) {
        FURI_LOG_E("ITS", "settings_save failed");
    }
    app->screen = AppScreenClock;
    app_refresh_clock_strings(app);
    app_commit_model(app, true);
}

static void app_sync_poll(App* app) {
    const uint32_t now = furi_get_tick();
    bool level = furi_hal_gpio_read(k_dcf77_pin);
    if(app->settings.dcf77_invert) {
        level = !level;
    }

    if(!app->gpio_have_level) {
        app->gpio_have_level = true;
        app->gpio_level = level;
        app->gpio_edge_tick = now;
        app->gpio_last_tick = now;
        return;
    }

    if(level != app->gpio_level) {
        const uint32_t width = now - app->gpio_edge_tick;
        /* Falling edge ends a carrier-reduction pulse (active-low modules). */
        if(app->gpio_level == false && level == true) {
            const Dcf77PulseKind kind = dcf77_classify_pulse_ms(width);
            if(dcf77_bit_buffer_feed(&app->dcf77_buf, kind)) {
                Dcf77CivilTime civil;
                if(dcf77_decode_frame(app->dcf77_buf.bits, &civil) &&
                   app_apply_civil_time(app, &civil)) {
                    snprintf(app->sync_status, sizeof(app->sync_status), "Synced");
                    snprintf(
                        app->sync_detail,
                        sizeof(app->sync_detail),
                        "%02u:%02u %s",
                        civil.hour,
                        civil.minute,
                        civil.cest ? "CEST" : "CET");
                    app_commit_model(app, true);
                    furi_delay_ms(800);
                    app_close_sync(app, false);
                    return;
                }
                snprintf(app->sync_status, sizeof(app->sync_status), "Bad frame");
                dcf77_bit_buffer_reset(&app->dcf77_buf);
            }
        }
        app->gpio_level = level;
        app->gpio_edge_tick = now;
    } else {
        const uint32_t gap = now - app->gpio_edge_tick;
        if(app->gpio_level == true && dcf77_classify_gap_ms(gap) == Dcf77PulseMinuteMark) {
            if(dcf77_bit_buffer_feed(&app->dcf77_buf, Dcf77PulseMinuteMark)) {
                Dcf77CivilTime civil;
                if(dcf77_decode_frame(app->dcf77_buf.bits, &civil) &&
                   app_apply_civil_time(app, &civil)) {
                    snprintf(app->sync_status, sizeof(app->sync_status), "Synced");
                    snprintf(
                        app->sync_detail,
                        sizeof(app->sync_detail),
                        "%02u:%02u %s",
                        civil.hour,
                        civil.minute,
                        civil.cest ? "CEST" : "CET");
                    app_commit_model(app, true);
                    furi_delay_ms(800);
                    app_close_sync(app, false);
                    return;
                }
                snprintf(app->sync_status, sizeof(app->sync_status), "Bad frame");
                dcf77_bit_buffer_reset(&app->dcf77_buf);
            }
            app->gpio_edge_tick = now; /* avoid re-trigger */
        }
    }

    if(now - app->gpio_last_tick >= 250) {
        app->gpio_last_tick = now;
        snprintf(
            app->sync_detail,
            sizeof(app->sync_detail),
            "bits %u/59",
            (unsigned)app->dcf77_buf.count);
        app_commit_model(app, true);
    }
}

static bool app_signal_callback(uint32_t signal, void* argument, void* context) {
    UNUSED(argument);
    App* app = context;
    if(signal == FuriSignalExit) {
        app_request_exit(app);
        return true;
    }
    return false;
}

static void app_handle_input(App* app, const InputEvent* event) {
    if(!app->running) {
        return;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong) {
        return;
    }

    if(app->screen == AppScreenClock) {
        if(event->key == InputKeyOk && event->type == InputTypeLong) {
            app_open_settings(app);
        } else if(
            (event->key == InputKeyOk || event->key == InputKeyBack) &&
            event->type == InputTypeShort) {
            app_request_exit(app);
        }
        return;
    }

    if(app->screen == AppScreenSync) {
        if(event->key == InputKeyBack && event->type == InputTypeShort) {
            app_close_sync(app, true);
        } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
            app_sync_demo_inject(app);
        }
        return;
    }

    /* Settings */
    if(event->key == InputKeyUp &&
       (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(app->settings_row > 0) {
            app->settings_row--;
        }
        app_commit_model(app, true);
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if((uint8_t)app->settings_row + 1 < AppSettingsRowCount) {
            app->settings_row++;
        }
        app_commit_model(app, true);
    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(app->settings_row == AppSettingsRowOffset) {
            app_adjust_offset(app, -1);
        } else if(app->settings_row == AppSettingsRowAuto) {
            app->settings.auto_dcf77_sync = !app->settings.auto_dcf77_sync;
            app_refresh_settings_labels(app);
        } else if(app->settings_row == AppSettingsRowInvert) {
            app->settings.dcf77_invert = !app->settings.dcf77_invert;
            app_refresh_settings_labels(app);
        }
        app_commit_model(app, true);
    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        if(app->settings_row == AppSettingsRowOffset) {
            app_adjust_offset(app, 1);
        } else if(app->settings_row == AppSettingsRowAuto) {
            app->settings.auto_dcf77_sync = !app->settings.auto_dcf77_sync;
            app_refresh_settings_labels(app);
        } else if(app->settings_row == AppSettingsRowInvert) {
            app->settings.dcf77_invert = !app->settings.dcf77_invert;
            app_refresh_settings_labels(app);
        }
        app_commit_model(app, true);
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(app->settings_row == AppSettingsRowSync) {
            app_open_sync(app);
        }
    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        app_close_settings(app);
    }
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

    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(AppViewModel));
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, app_draw_callback);
    view_set_input_callback(app->view, app_view_input_callback);

    app->view_holder = view_holder_alloc();
    view_holder_attach_to_gui(app->view_holder, app->gui);
    view_holder_set_back_callback(app->view_holder, app_back_callback, app);
    view_holder_set_view(app->view_holder, app->view);
    view_holder_send_to_front(app->view_holder);

    app->running = true;
    if(!app->settings.loaded) {
        app->settings.utc_offset_minutes = 0;
        app->settings.loaded = true;
        (void)settings_save(app->storage, &app->settings);
    }
    app->screen = AppScreenClock;
    app_refresh_clock_strings(app);
    app_commit_model(app, true);
    return app;
}

static void app_free(App* app) {
    furi_check(app);
    app->running = false;
    if(app->screen == AppScreenSync) {
        app_gpio_deinit();
    }
    if(app->view_holder) {
        view_holder_set_view(app->view_holder, NULL);
        view_holder_free(app->view_holder);
        app->view_holder = NULL;
    }
    if(app->view) {
        view_free(app->view);
        app->view = NULL;
    }
    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }
    if(app->event_queue) {
        furi_message_queue_free(app->event_queue);
        app->event_queue = NULL;
    }
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }
    free(app);
}

int32_t app_run(void* p) {
    UNUSED(p);

    App* app = app_alloc();
    furi_thread_set_signal_callback(furi_thread_get_current(), app_signal_callback, app);

    /* Auto-sync once per session when stale. */
    DateTime now_dt;
    furi_hal_rtc_get_datetime(&now_dt);
    const uint32_t now_epoch = datetime_datetime_to_timestamp(&now_dt);
    if(dcf77_should_auto_sync(
           now_epoch,
           app->settings.last_dcf77_sync_epoch,
           app->settings.auto_dcf77_sync,
           DCF77_AUTO_SYNC_INTERVAL_S)) {
        app->auto_sync_attempted = true;
        app_open_sync(app);
    }

    AppEvent event;
    uint32_t last_second = 61;
    while(app->running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 50);
        if(status == FuriStatusOk) {
            if(event.type == AppEventTypeExit) {
                break;
            }
            if(event.type == AppEventTypeInput) {
                app_handle_input(app, &event.input);
            }
        }
        if(!app->running) {
            break;
        }

        if(app->screen == AppScreenSync) {
            app_sync_poll(app);
        } else if(app->screen == AppScreenClock) {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            if(dt.second != last_second) {
                last_second = dt.second;
                app_refresh_clock_strings(app);
                app_commit_model(app, true);
            }
        }
    }

    furi_thread_set_signal_callback(furi_thread_get_current(), NULL, NULL);
    app_free(app);
    return 0;
}
