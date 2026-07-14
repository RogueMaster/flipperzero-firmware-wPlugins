/**
 * timer_logic_test_only.h - テスト用の簡略化されたタイマーロジック実装
 * 
 * このファイルはテスト専用で、実際のアプリケーションでは使用されません。
 * timer_logic_test.cからインクルードされ、テストを実行するために使用されます。
 */

#ifndef TIMER_LOGIC_TEST_ONLY_H
#define TIMER_LOGIC_TEST_ONLY_H

#include <stdint.h>

/**
 * 現在時刻から次のターゲット時刻（XX:50:00 または XX+1:00:00）までの残り秒数を計算する
 */
static inline int timer_logic_get_countdown_seconds_test(int hour, int min, int sec) {
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
 */
static inline int timer_logic_should_trigger_alarm_test(int hour, int min, int sec) {
    (void)hour; // Explicitly mark as unused
    // XX:50:00またはXX:00:00の場合にアラームをトリガー
    return ((min == 50 && sec == 0) || (min == 0 && sec == 0)) ? 1 : 0;
}

#endif // TIMER_LOGIC_TEST_ONLY_H
