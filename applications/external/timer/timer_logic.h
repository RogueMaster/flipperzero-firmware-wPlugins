#ifndef TIMER_LOGIC_H
#define TIMER_LOGIC_H

#include "timer_app.h"

// epoch から「時」と「分」を取得するユーティリティ
static inline void get_hour_minute(time_t epoch, int* hour, int* minute) {
    *minute = (epoch / 60) % 60;
    *hour = (epoch / 3600) % 24;
}

// epoch を UTCブレイクダウンするユーティリティ
static inline void epoch_to_utc_breakdown(
    time_t epoch,
    int* year,
    int* mon,
    int* day,
    int* wday,
    int* hour,
    int* min,
    int* sec) {
    *sec = epoch % 60;
    epoch /= 60;
    *min = epoch % 60;
    epoch /= 60;
    *hour = epoch % 24;
    epoch /= 24;
    int days_since_1970 = (int)epoch;
    *wday = (days_since_1970 + 4) % 7;
    int y = 1970;
    while(true) {
        int y_len = ((y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365);
        if(days_since_1970 >= y_len) {
            days_since_1970 -= y_len;
            y++;
        } else {
            break;
        }
    }
    *year = y;
    static const int MDAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m;
    for(m = 0; m < 12; m++) {
        int dmax = MDAYS[m];
        if(m == 1 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) {
            dmax = 29;
        }
        if(days_since_1970 >= dmax) {
            days_since_1970 -= dmax;
        } else {
            break;
        }
    }
    *mon = m + 1;
    *day = days_since_1970 + 1;
}

/**
 * 現在時刻から次のターゲット時刻（XX:50:00 または XX+1:00:00）までの残り秒数を計算する
 * 
 * @param hour 現在の時 (0-23)
 * @param min 現在の分 (0-59)
 * @param sec 現在の秒 (0-59)
 * @return 次のターゲット時刻までの残り秒数。ターゲット時刻に達した場合は0を返す
 */
int timer_logic_get_countdown_seconds(int hour, int min, int sec);

/**
 * 現在時刻がアラームをトリガーすべき時刻かどうかを判定する
 * 
 * @param hour 現在の時 (0-23)
 * @param min 現在の分 (0-59)
 * @param sec 現在の秒 (0-59)
 * @return アラームをトリガーすべき場合は1、そうでない場合は0
 */
int timer_logic_should_trigger_alarm(int hour, int min, int sec);

void update_daily_record(PomodoroApp* app, time_t now);
void save_daily_records(PomodoroApp* app);
void load_daily_records(PomodoroApp* app);
void update_logic(PomodoroApp* app);

#endif // TIMER_LOGIC_H
