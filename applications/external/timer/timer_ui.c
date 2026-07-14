#include "timer_ui.h"
#include "timer_logic.h" // get_hour_minute, epoch_to_utc_breakdown を使用
#include "timer_alarm.h" // stop_alarm_sound, start_screen_blink, show_dialog などを使用
#include <math.h>

// 曜日の略称（draw_callback 用）
static const char* WDAY_NAME[7] = {"S", "M", "T", "W", "T", "F", "S"};

void draw_thick_line(Canvas* canvas, int x0, int y0, int x1, int y1, int thickness) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    float length = sqrt(dx * dx + dy * dy);
    if(length == 0) return;
    float ux = (float)dy / length;
    float uy = -(float)dx / length;
    int half = thickness / 2;
    for(int i = -half; i <= half; i++) {
        int offset_x = (int)round(i * ux);
        int offset_y = (int)round(i * uy);
        canvas_draw_line(canvas, x0 + offset_x, y0 + offset_y, x1 + offset_x, y1 + offset_y);
    }
}

void draw_small_analog_clock(Canvas* canvas, time_t now) {
    int center_x = 15, center_y = 15, radius = 15;
    canvas_draw_circle(canvas, center_x, center_y, radius);
    for(int i = 0; i < 12; i++) {
        float tick_angle = i * (2 * M_PI / 12) - (M_PI / 2);
        int tick_inner = radius - 2, tick_outer = radius;
        int x_start = center_x + (int)(tick_inner * cos(tick_angle));
        int y_start = center_y + (int)(tick_inner * sin(tick_angle));
        int x_end = center_x + (int)(tick_outer * cos(tick_angle));
        int y_end = center_y + (int)(tick_outer * sin(tick_angle));
        canvas_draw_line(canvas, x_start, y_start, x_end, y_end);
    }
    int year, mon, day, wday, hour, minute, second;
    epoch_to_utc_breakdown(now, &year, &mon, &day, &wday, &hour, &minute, &second);
    int hour_mod = hour % 12;
    float hour_angle = ((hour_mod + minute / 60.0f) / 12.0f) * 2 * M_PI - (M_PI / 2);
    float minute_angle = ((minute + second / 60.0f) / 60.0f) * 2 * M_PI - (M_PI / 2);
    int hour_hand_length = radius - 4, minute_hand_length = radius - 2;
    int hour_x_end = center_x + (int)(hour_hand_length * cos(hour_angle));
    int hour_y_end = center_y + (int)(hour_hand_length * sin(hour_angle));
    int minute_x_end = center_x + (int)(minute_hand_length * cos(minute_angle));
    int minute_y_end = center_y + (int)(minute_hand_length * sin(minute_angle));
    draw_thick_line(canvas, center_x, center_y, hour_x_end, hour_y_end, 1);
    draw_thick_line(canvas, center_x, center_y, minute_x_end, minute_y_end, 1);
}

void draw_callback(Canvas* canvas, void* ctx) {
    PomodoroApp* app = (PomodoroApp*)ctx;
    if(app->record_view_active) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Daily Record");
        int y_pos = 20;
        for(int i = 0; i < app->daily_record_count; i++) {
            char buf[64];
            snprintf(
                buf,
                sizeof(buf),
                "%04d-%02d-%02d: S:%d R:%d F:%d",
                app->daily_records[i].year,
                app->daily_records[i].month,
                app->daily_records[i].day,
                app->daily_records[i].start_count,
                app->daily_records[i].rest_count,
                app->daily_records[i].fail_count);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignTop, buf);
            y_pos += 10;
            if(y_pos > 55) break;
        }
        {
            char buf[64];
            snprintf(
                buf,
                sizeof(buf),
                "%04d-%02d-%02d: S:%d R:%d F:%d",
                app->current_daily_record.year,
                app->current_daily_record.month,
                app->current_daily_record.day,
                app->current_daily_record.start_count,
                app->current_daily_record.rest_count,
                app->current_daily_record.fail_count);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignTop, buf);
        }
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "[Left] Exit");
        return;
    }
    canvas_clear(canvas);
    time_t now = furi_hal_rtc_get_timestamp();
    draw_small_analog_clock(canvas, now);
    {
        int y, mo, d, wd, hh, mm, ss;
        epoch_to_utc_breakdown(now, &y, &mo, &d, &wd, &hh, &mm, &ss);
        char date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %s", y, mo, d, WDAY_NAME[wd]);
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d", hh, mm, ss);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, date_buf);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignTop, time_buf);
    }
    if(app->dialog_active) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, app->dialog_title);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, app->dialog_message);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, "[OK:Up]");
    } else {
        int hour, minute;
        get_hour_minute(now, &hour, &minute);
        canvas_set_font(canvas, FontSecondary);

        // カウントダウン秒数を表示
        int countdown_seconds = app->state.countdown_seconds;
        int minutes = countdown_seconds / 60;
        int seconds = countdown_seconds % 60;
        char buf[32];

        if(app->state.phase == PomodoroPhaseIdle) {
            // XX:00:00までのカウントダウン（次の開始時刻）
            snprintf(buf, sizeof(buf), "Start in %d:%02d", minutes, seconds);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, buf);
        } else {
            // XX:50:00までのカウントダウン（次の休憩時刻）
            snprintf(buf, sizeof(buf), "Rest in %d:%02d", minutes, seconds);
            canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, buf);
        }
        canvas_draw_str_aligned(canvas, 128, 1, AlignRight, AlignTop, "[Right]:Test");
    }
}

void input_callback(InputEvent* event, void* ctx) {
    PomodoroApp* app = (PomodoroApp*)ctx;
    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyBack:
            stop_alarm_sound(app);
            app->running = false;
            break;
        case InputKeyUp:
            if(app->dialog_active) {
                app->dialog_result = true;
                app->dialog_active = false;
                stop_alarm_sound(app);
                app->blinking = false;
            }
            break;
        case InputKeyRight:
            if(app->state.phase == PomodoroPhaseIdle) {
                start_screen_blink(app, 3000);
                show_dialog(
                    app,
                    "Start Alarm (Test)",
                    "Simulate Start.\nPress UP to confirm.",
                    10 * 1000,
                    AlarmTypeStart);
                app->hourly_alarm_active = true;
            } else if(app->state.phase == PomodoroPhaseWaitingRest) {
                start_screen_blink(app, 3000);
                show_dialog(
                    app,
                    "Rest Alarm (Test)",
                    "Simulate Rest.\nPress UP to confirm.",
                    10 * 1000,
                    AlarmTypeRest);
                app->state.phase = PomodoroPhaseIdle;
                app->state.next_rest_time = 0;
            }
            break;
        case InputKeyLeft:
            if(!app->dialog_active) {
                app->record_view_active = !app->record_view_active;
            }
            break;
        default:
            break;
        }
    }
}
