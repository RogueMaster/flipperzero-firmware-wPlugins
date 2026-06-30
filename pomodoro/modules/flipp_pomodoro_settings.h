#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FlippPomodoroBuzzSlide = 0,
    FlippPomodoroBuzzOnce = 1,
    FlippPomodoroBuzzAnnoying = 2,
    FlippPomodoroBuzzFlash = 3,
    FlippPomodoroBuzzVibrate = 4,
    FlippPomodoroBuzzSoftBeep = 5,
    FlippPomodoroBuzzLoudBeep = 6,
    FlippPomodoroBuzzModeCount,
} FlippPomodoroBuzzMode;

typedef struct {
    uint8_t focus_minutes;
    uint8_t short_break_minutes;
    uint8_t long_break_minutes;
    uint8_t buzz_mode;
} FlippPomodoroSettings;

bool flipp_pomodoro_settings_load(FlippPomodoroSettings* settings);
bool flipp_pomodoro_settings_save(const FlippPomodoroSettings* settings);
void flipp_pomodoro_settings_set_default(FlippPomodoroSettings* settings);
