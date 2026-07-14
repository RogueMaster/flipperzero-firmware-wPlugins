#ifndef TIMER_APP_H
#define TIMER_APP_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Furi関連のヘッダ群
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <furi_hal_speaker.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

// 記録ファイルのパス
#define RECORD_FILE APP_DATA_PATH("pomodoro_records.dat")

// ポモドーロのフェーズ
typedef enum {
    PomodoroPhaseIdle = 0, // 次の XX:00 で開始アラーム
    PomodoroPhaseWaitingRest, // 次の XX:50 で休憩アラーム
} PomodoroPhase;

// アラーム種類
typedef enum {
    AlarmTypeNone = 0,
    AlarmTypeStart,
    AlarmTypeRest,
} AlarmType;

// 日毎の実績記録用構造体
typedef struct {
    int year;
    int month;
    int day;
    int start_count;
    int rest_count;
    int fail_count;
} DailyRecord;

// ポモドーロ状態構造体
typedef struct {
    PomodoroPhase phase;
    time_t next_rest_time;
    int last_hour_triggered; // 最後にアラームを出した「時」(0-23)、-1なら未設定
    int countdown_seconds; // 次のターゲット時刻までの残り秒数
} PomodoroState;

// アプリ全体管理構造体
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    bool running;
    PomodoroState state;

    // ダイアログ関連
    bool dialog_active;
    bool dialog_result;
    uint32_t dialog_timeout;
    uint32_t dialog_start;
    char dialog_title[32];
    char dialog_message[64];
    AlarmType current_alarm_type;

    // バックライト（LED）は使用せず、バックライト関連はダミー
    bool blinking;
    uint32_t blink_start;
    uint32_t blink_duration;
    uint32_t last_blink_toggle;
    bool backlight_on;

    // 毎時アラームのフラグ
    bool hourly_alarm_active;

    // カウンタ（表示には使用しない）
    int start_count;
    int rest_count;
    int fail_count;

    // 日毎の実績記録用フィールド
    bool record_view_active;
    DailyRecord current_daily_record;
    DailyRecord daily_records[30];
    int daily_record_count;

    // アラーム音が再生中かどうかのフラグ
    bool alarm_sound_active;
} PomodoroApp;

// アプリのエントリーポイント
int32_t pomodoro_app_main(void* p);

#endif // TIMER_APP_H
