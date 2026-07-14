#ifndef TIMER_ALARM_H
#define TIMER_ALARM_H

#include "timer_app.h"

void stop_alarm_sound(PomodoroApp* app);
void start_screen_blink(PomodoroApp* app, uint32_t duration_ms);
void toggle_backlight_with_vibro(PomodoroApp* app);
void show_dialog(
    PomodoroApp* app,
    const char* title,
    const char* message,
    uint32_t timeout_ms,
    AlarmType alarm_type);

#endif // TIMER_ALARM_H
