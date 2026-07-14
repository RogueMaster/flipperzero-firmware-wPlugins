#include "timer_logic.h"
#include "timer_alarm.h" // アラーム関連の関数を使用するため

/**
 * 現在時刻から次のターゲット時刻（XX:50:00 または XX+1:00:00）までの残り秒数を計算する
 * 
 * @param hour 現在の時 (0-23)
 * @param min 現在の分 (0-59)
 * @param sec 現在の秒 (0-59)
 * @return 次のターゲット時刻までの残り秒数。ターゲット時刻に達した場合は0を返す
 */
int timer_logic_get_countdown_seconds(int hour, int min, int sec) {
    (void)hour; // Explicitly mark as unused
    // 現在時刻の秒数（時間内）
    int seconds_of_hour = min * 60 + sec;

    // ターゲット時刻の秒数
    int target_seconds;

    // XX:00:00の場合、ターゲット時刻に達している
    if(min == 0 && sec == 0) {
        return 0;
    }
    // XX:50:00の場合、ターゲット時刻に達している
    else if(min == 50 && sec == 0) {
        return 0;
    }
    // XX:00:01～XX:49:59の場合、ターゲットはXX:50:00
    else if(seconds_of_hour < 50 * 60) {
        target_seconds = 50 * 60; // XX:50:00
        return target_seconds - seconds_of_hour;
    }
    // XX:50:01～XX:59:59の場合、ターゲットは(XX+1):00:00
    else {
        target_seconds = 60 * 60; // (XX+1):00:00
        return target_seconds - seconds_of_hour;
    }
}

/**
 * 現在時刻がアラームをトリガーすべき時刻かどうかを判定する
 * 
 * @param hour 現在の時 (0-23)
 * @param min 現在の分 (0-59)
 * @param sec 現在の秒 (0-59)
 * @return アラームをトリガーすべき場合は1、そうでない場合は0
 */
int timer_logic_should_trigger_alarm(int hour, int min, int sec) {
    (void)hour; // Explicitly mark as unused
    // XX:50:00またはXX:00:00の場合にアラームをトリガー
    return ((min == 50 && sec == 0) || (min == 0 && sec == 0)) ? 1 : 0;
}

// 日毎の記録データ保存
void save_daily_records(PomodoroApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RECORD_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &app->daily_record_count, sizeof(app->daily_record_count));
        storage_file_write(file, app->daily_records, sizeof(app->daily_records));
        storage_file_write(file, &app->current_daily_record, sizeof(app->current_daily_record));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// 日毎の記録データ読み込み
void load_daily_records(PomodoroApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, RECORD_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &app->daily_record_count, sizeof(app->daily_record_count));
        storage_file_read(file, app->daily_records, sizeof(app->daily_records));
        storage_file_read(file, &app->current_daily_record, sizeof(app->current_daily_record));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// 日毎の記録更新
void update_daily_record(PomodoroApp* app, time_t now) {
    int year, mon, day, dummy, dummy2, dummy3, dummy4;
    epoch_to_utc_breakdown(now, &year, &mon, &day, &dummy, &dummy2, &dummy3, &dummy4);
    if(year != app->current_daily_record.year || mon != app->current_daily_record.month ||
       day != app->current_daily_record.day) {
        if(app->daily_record_count < 30) {
            app->daily_records[app->daily_record_count++] = app->current_daily_record;
        }
        app->current_daily_record.year = year;
        app->current_daily_record.month = mon;
        app->current_daily_record.day = day;
        app->current_daily_record.start_count = 0;
        app->current_daily_record.rest_count = 0;
        app->current_daily_record.fail_count = 0;
        save_daily_records(app);
    }
}

// アプリ全体の状態更新ロジック
void update_logic(PomodoroApp* app) {
    time_t now = furi_hal_rtc_get_timestamp();
    update_daily_record(app, now);
    int hour, minute, second;
    get_hour_minute(now, &hour, &minute);
    second = now % 60;

    if(app->blinking) {
        uint32_t now_tick = furi_get_tick();
        uint32_t elapsed = now_tick - app->blink_start;
        if(elapsed >= app->blink_duration) {
            if(!app->backlight_on) {
                toggle_backlight_with_vibro(app);
            }
            app->blinking = false;
        } else {
            if((now_tick - app->last_blink_toggle) >= 500) {
                toggle_backlight_with_vibro(app);
                app->last_blink_toggle = now_tick;
            }
        }
    }

    if(app->dialog_active) {
        uint32_t elapsed = furi_get_tick() - app->dialog_start;
        if(elapsed >= app->dialog_timeout) {
            app->fail_count++;
            app->current_daily_record.fail_count++;
            app->dialog_active = false;
            stop_alarm_sound(app);
            app->blinking = false;
            save_daily_records(app);
        }
        return;
    } else {
        if(app->dialog_result) {
            stop_alarm_sound(app);
            app->blinking = false;
            if(app->current_alarm_type == AlarmTypeStart) {
                app->start_count++;
                app->current_daily_record.start_count++;
                app->state.phase = PomodoroPhaseWaitingRest;
                int current_total = hour * 60 + minute;
                int target_total;
                if(minute < 50) {
                    target_total = hour * 60 + 50;
                } else {
                    target_total = (hour + 1) * 60;
                }
                int diff = target_total - current_total;
                app->state.next_rest_time = now + diff * 60;
            } else if(app->current_alarm_type == AlarmTypeRest) {
                app->rest_count++;
                app->current_daily_record.rest_count++;
                app->state.phase = PomodoroPhaseIdle;
                app->state.next_rest_time = 0;
            }
            app->dialog_result = false;
            app->current_alarm_type = AlarmTypeNone;
            save_daily_records(app);
        }
    }

    // 新しいカウントダウンロジック
    int countdown_seconds = timer_logic_get_countdown_seconds(hour, minute, second);

    // カウントダウン秒数をアプリ状態に保存（UI表示などに使用）
    app->state.countdown_seconds = countdown_seconds;

    // アラームトリガーチェック
    if(timer_logic_should_trigger_alarm(hour, minute, second) && !app->dialog_active) {
        AlarmType alarm_type;
        const char* title;
        const char* message;

        // XX:50:00の場合はスタートアラーム、XX:00:00の場合は休憩アラーム
        if(minute == 50) {
            alarm_type = AlarmTypeStart;
            title = "Start Alarm";
            message = "Time to start?\nPress UP to confirm.";
        } else { // minute == 0
            alarm_type = AlarmTypeRest;
            title = "Rest Alarm";
            message = "Time to rest?\nPress UP to confirm.";
        }

        show_dialog(app, title, message, 10 * 1000, alarm_type);
        start_screen_blink(app, 3000);
    }

    // ここでカウントダウン秒数を使用して表示を更新するなどの処理を行う
    // （UIの更新はtimer_ui.cで行われる可能性があるため、ここでは変数を更新するだけ）
}
